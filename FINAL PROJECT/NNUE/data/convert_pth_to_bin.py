import struct
import torch

WEIGHTS_FILE = "weights.pth"
OUT_FILE = "nnue_weights.bin"

BN_EPS = 1e-5  # must match nn.BatchNorm1d's default eps used during training

# Must match nnue.hpp's WEIGHTS_MAGIC / IN_DIM / H1 / H2 / H3 exactly. Written
# as a header so the C++ loader can reject a file that doesn't match its own
# compiled-in architecture instead of silently reading a stale/wrong-shaped
# binary (previously this could only be caught by luck, if the file happened
# to run out of bytes at just the right point).
WEIGHTS_MAGIC = 0x4E4E5545
IN_DIM, H1, H2, H3 = 781, 512, 256, 128

state = torch.load(WEIGHTS_FILE, map_location="cpu")

# fc1/fc2/fc3 are each followed by a BatchNorm1d in training; fc4 (the output
# head) has none. nnue.hpp has zero knowledge of BatchNorm -- it only knows
# how to do Linear -> ReLU -- so we fold each BN's running statistics into
# its preceding Linear layer's weight/bias here, at export time. The result
# is numerically identical to Linear -> BN(eval) -> ReLU, but comes out as a
# plain Linear -> ReLU, so nnue.hpp and the binary format need no changes.
layers = [
    ("fc1", "bn1"),
    ("fc2", "bn2"),
    ("fc3", "bn3"),
    ("fc4", None),
]


def fold_batchnorm(weight, bias, bn_prefix, state):
    """
    BatchNorm1d in eval mode computes, per output channel o:
        y[o] = gamma[o] * (z[o] - running_mean[o]) / sqrt(running_var[o] + eps) + beta[o]
    where z = weight @ x + bias is the preceding Linear's output. Expanding:
        y[o] = scale[o] * z[o] + shift[o]
        scale[o] = gamma[o] / sqrt(running_var[o] + eps)
        shift[o] = beta[o] - running_mean[o] * scale[o]
    Since z[o] = weight[o,:] @ x + bias[o], substituting gives a new Linear:
        folded_weight[o,:] = scale[o] * weight[o,:]
        folded_bias[o]     = scale[o] * bias[o] + shift[o]
    This is just per-output-row scaling, so it's still linear in x -- the
    incremental accumulator in nnue.hpp (which sums w1 columns per piece)
    stays mathematically valid with the folded weights.
    """
    gamma = state[f"{bn_prefix}.weight"]
    beta = state[f"{bn_prefix}.bias"]
    running_mean = state[f"{bn_prefix}.running_mean"]
    running_var = state[f"{bn_prefix}.running_var"]

    scale = gamma / torch.sqrt(running_var + BN_EPS)
    shift = beta - running_mean * scale

    folded_weight = weight * scale.unsqueeze(1)
    folded_bias = bias * scale + shift

    return folded_weight, folded_bias


with open(OUT_FILE, "wb") as f:
    f.write(struct.pack("<Iiiii", WEIGHTS_MAGIC, IN_DIM, H1, H2, H3))

    for fc_name, bn_name in layers:
        w = state[f"{fc_name}.weight"].detach().cpu()

        bias_key = f"{fc_name}.bias"
        if bias_key in state:
            b = state[bias_key].detach().cpu()
        else:
            # fc1/fc2/fc3 are bias=False (BatchNorm's beta supplies the
            # shift instead) -- fold_batchnorm still works fine starting
            # from an all-zero bias here.
            b = torch.zeros(w.shape[0])

        if bn_name is not None:
            w, b = fold_batchnorm(w, b, bn_name, state)
            print(f"{fc_name}: folded {bn_name} into weight/bias")

        w = w.numpy().astype("float32")
        b = b.numpy().astype("float32")

        # sanity check: nn.Linear weight is (out_features, in_features), row-major.
        # The C++ side reads this back in the exact same flat row-major order.
        f.write(w.tobytes())
        f.write(b.tobytes())

        print(f"{fc_name}: weight {tuple(w.shape)}  bias {tuple(b.shape)}")

print(f"\nExported to {OUT_FILE}")
