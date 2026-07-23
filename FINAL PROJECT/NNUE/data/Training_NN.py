import os
import sys
import time
import copy
import torch
import torch.nn as nn


DEVICE=torch.device("cpu")

# make sure torch actually uses all your CPU cores for the matmuls --
# this is the CPU analogue of "using more cores", unlike test.py this
# doesn't need separate processes since torch's BLAS backend already
# parallelizes internally, it just needs to be told how many threads to use
torch.set_num_threads(os.cpu_count())

BATCH_SIZE=4096
EPOCHS=60          # early stopping below will almost certainly cut this short --
                   # this is just a ceiling, not how long training will actually run
LR=3e-3            # BatchNorm smooths the loss landscape, so this can go higher than
                   # the old 1e-3 -- if you see loss spikes/NaNs, back this off
WEIGHT_DECAY=1e-2  # AdamW decouples this from the gradient update (unlike Adam's L2 term)
DROPOUT_P=0.2      # applied after fc1 and fc2 only -- see NNUE.__init__ comment
EARLY_STOP_PATIENCE=15  # stop if val RMSE hasn't improved in this many epochs

ASK_EACH_EPOCH=False   # set True if you want a Y/n prompt between epochs


piece_indices=torch.load("piece_indices.pt")
castle=torch.load("castle.pt")
ep=torch.load("ep.pt")
targets=torch.load("targets.pt")


N=piece_indices.size(0)

train_size=int(0.9*N)
val_size=N-train_size


# Manual index-based split/batching instead of TensorDataset + DataLoader.
# These tensors are already fully loaded in memory, but DataLoader's default
# path (num_workers=0) still calls dataset[i] one sample at a time in a
# Python loop, then collates -- built for cases like reading files off disk,
# not slicing tensors that are already sitting in RAM. Benchmarked at ~15x
# slower per batch than just indexing the tensors directly, which was
# quietly eating a big chunk of every epoch regardless of the model itself.
split_perm=torch.randperm(N,generator=torch.Generator().manual_seed(42))

train_idx=split_perm[:train_size]
val_idx=split_perm[train_size:]


def iterate_batches(indices,batch_size,shuffle,drop_last):

    if shuffle:
        indices=indices[torch.randperm(indices.size(0))]

    n=indices.size(0)

    n_batches=n//batch_size if drop_last else -(-n//batch_size)  # ceil div when keeping the tail

    for i in range(n_batches):

        batch_idx=indices[i*batch_size:(i+1)*batch_size]

        yield (
            piece_indices[batch_idx],
            castle[batch_idx],
            ep[batch_idx],
            targets[batch_idx],
        )



class NNUE(nn.Module):

    def __init__(self):

        super().__init__()

        # bias=False on every Linear feeding into a BatchNorm: BN's own beta
        # already acts as a learned shift, so a preceding bias term is just
        # redundant weight for the optimizer to push around. fc4 has no BN
        # after it, so it keeps its bias.
        self.fc1=nn.Linear(781,512,bias=False)
        self.fc2=nn.Linear(512,256,bias=False)
        self.fc3=nn.Linear(256,128,bias=False)
        self.fc4=nn.Linear(128,1)

        # BatchNorm sits between each Linear and its ReLU (the standard order).
        # No BN on fc4: it's the final regression head, and normalizing a
        # single scalar output has no batch-statistics benefit.
        self.bn1=nn.BatchNorm1d(512)
        self.bn2=nn.BatchNorm1d(256)
        self.bn3=nn.BatchNorm1d(128)

        self.relu=nn.ReLU()

        # Dropout only after fc1/fc2 -- the two widest layers, and the ones
        # most likely to overfit. Left off fc3->fc4 so the last hidden layer
        # feeding the output head stays stable.
        self.dropout1=nn.Dropout(DROPOUT_P)
        self.dropout2=nn.Dropout(DROPOUT_P)

        # Kaiming/He-normal init on every layer that feeds a ReLU (fc1-fc3).
        # fc4 is a linear regression head with no activation after it, so it's
        # left on PyTorch's default init, same as the Dense(1) in the screenshot.
        nn.init.kaiming_normal_(self.fc1.weight,nonlinearity="relu")
        nn.init.kaiming_normal_(self.fc2.weight,nonlinearity="relu")
        nn.init.kaiming_normal_(self.fc3.weight,nonlinearity="relu")


    def forward(self,x):

        x=self.dropout1(self.relu(self.bn1(self.fc1(x))))
        x=self.dropout2(self.relu(self.bn2(self.fc2(x))))
        x=self.relu(self.bn3(self.fc3(x)))
        x=self.fc4(x)

        return x.squeeze(1)



SQUASH_K=400.0    # Stockfish-style scaling constant


def squashed_mse(pred_cp,target_cp):
    """
    Training loss operates in squashed (sigmoid) space rather than raw
    centipawns. This keeps gradients well-behaved regardless of how large
    the target cp value is -- a 900 vs 1000 cp target should barely matter
    to the network, but raw MSE would still push hard on that 100cp gap.
    Reported RMSE elsewhere in this file stays in raw cp units; this
    function is only used for backprop.
    """

    pred_squash=torch.sigmoid(pred_cp/SQUASH_K)
    target_squash=torch.sigmoid(target_cp/SQUASH_K)

    return nn.functional.mse_loss(pred_squash,target_squash)



def make_features(pieces,castle,ep):

    batch_size=pieces.size(0)

    x=torch.zeros(
        batch_size,
        781,
        device=pieces.device
    )


    # columns 0-767: piece features (stm-relative, from test.py)
    # column 768: dummy pad slot (PAD_IDX target) -- intentionally not
    #             read as a meaningful feature, just left for the net
    #             to learn to ignore. This used to collide with an
    #             explicit stm bit, which corrupted that feature.
    x.scatter_(
        1,
        pieces,
        1.0
    )


    x[:,769:773]=castle.float()

    x[:,773:781]=ep.float()


    return x



def evaluate(model,indices):

    model.eval()

    total_loss=0.0
    total_count=0


    with torch.no_grad():

        for pieces,castle_batch,ep_batch,target in iterate_batches(indices,BATCH_SIZE,shuffle=False,drop_last=False):

            pieces=pieces.to(DEVICE)
            castle_batch=castle_batch.to(DEVICE)
            ep_batch=ep_batch.to(DEVICE)
            target=target.to(DEVICE)


            x=make_features(
                pieces,
                castle_batch,
                ep_batch
            )


            pred=model(x)


            loss=nn.functional.mse_loss(
                pred,
                target,
                reduction="sum"
            )


            total_loss+=loss.item()

            total_count+=target.size(0)


    return (total_loss/total_count)**0.5



model=NNUE().to(DEVICE)


optimizer=torch.optim.AdamW(
    model.parameters(),
    lr=LR,
    weight_decay=WEIGHT_DECAY
)


scheduler=torch.optim.lr_scheduler.ReduceLROnPlateau(
    optimizer,
    mode="min",
    factor=0.5,
    patience=2,
    threshold=1e-3,
    min_lr=1e-6
)



print(f"Training positions : {train_idx.size(0):,}")
print(f"Validation positions : {val_idx.size(0):,}")

total_params=sum(
    p.numel()
    for p in model.parameters()
)

print(f"Parameters : {total_params:,}")


best_val_rmse=float("inf")
best_state_dict=None
epochs_since_improvement=0



for epoch in range(EPOCHS):

    model.train()

    total_loss=0.0

    start=time.time()

    next_percent=1

    total_batches=train_idx.size(0)//BATCH_SIZE


    print(f"\nEpoch {epoch+1}/{EPOCHS}")


    for batch_idx,(pieces,castle_batch,ep_batch,target) in enumerate(iterate_batches(train_idx,BATCH_SIZE,shuffle=True,drop_last=True)):


        pieces=pieces.to(DEVICE)
        castle_batch=castle_batch.to(DEVICE)
        ep_batch=ep_batch.to(DEVICE)
        target=target.to(DEVICE)


        x=make_features(
            pieces,
            castle_batch,
            ep_batch
        )


        optimizer.zero_grad(
            set_to_none=True
        )


        pred=model(x)


        loss=squashed_mse(
            pred,
            target
        )


        loss.backward()

        optimizer.step()


        with torch.no_grad():
            cp_sq_err=nn.functional.mse_loss(
                pred,
                target,
                reduction="sum"
            ).item()

        total_loss+=cp_sq_err



        progress=(batch_idx+1)*100//total_batches


        while progress>=next_percent:

            processed=min(
                (batch_idx+1)*BATCH_SIZE,
                train_idx.size(0)
            )


            avg_loss=total_loss/processed

            rmse=avg_loss**0.5


            elapsed=time.time()-start

            speed=processed/max(elapsed,1e-9)

            eta=(train_idx.size(0)-processed)/max(speed,1e-9)


            print(
                f"{next_percent:3d}% "
                f"RMSE={rmse:.3f} "
                f"ETA={eta:.1f}s"
            )


            next_percent+=1



    train_rmse=(total_loss/train_idx.size(0))**0.5


    val_rmse=evaluate(
        model,
        val_idx
    )


    scheduler.step(
        val_rmse**2
    )


    print(
        f"Epoch {epoch+1} complete "
        f"Train RMSE={train_rmse:.3f} "
        f"Val RMSE={val_rmse:.3f} "
        f"LR={optimizer.param_groups[0]['lr']:.6e} "
        f"Time={time.time()-start:.1f}s"
    )


    if val_rmse<best_val_rmse:

        best_val_rmse=val_rmse

        best_state_dict=copy.deepcopy(model.state_dict())

        epochs_since_improvement=0

    else:

        epochs_since_improvement+=1

        if epochs_since_improvement>=EARLY_STOP_PATIENCE:

            print(
                f"\nEarly stopping: val RMSE hasn't improved in "
                f"{EARLY_STOP_PATIENCE} epochs (best={best_val_rmse:.3f}). "
                f"Restoring best weights."
            )

            break


    if epoch+1<EPOCHS and ASK_EACH_EPOCH:

        if sys.stdin.isatty():

            answer=input("Continue to next epoch? [Y/n]: ").strip().lower()

            if answer.startswith("n"):

                print("Stopping early -- saving current weights.")

                break

        else:

            print("Non-interactive session detected -- continuing automatically.")



if best_state_dict is not None:

    model.load_state_dict(best_state_dict)

    print(f"\nRestored best weights (Val RMSE={best_val_rmse:.3f})")


torch.save(
    model.state_dict(),
    "weights.pth"
)


print("\nSaved weights to weights.pth")
