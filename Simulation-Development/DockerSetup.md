# Docker & ROS 2 Environment Setup Guide

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Windows Setup](#windows-setup)
3. [Linux / WSL Setup](#linux--wsl-setup)
4. [Running the Simulation](#running-the-simulation)
5. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### What is Docker?

**Docker** is a containerization platform that packages applications and all their dependencies into isolated, portable containers. Think of it as a lightweight virtual machine that ensures software runs identically across any system — whether it's Windows, Linux, or macOS.

**Note**: You don't need to understand Docker in detail to follow this guide. Simply treat it as a tool that makes the ROS 2 + RViz2 simulation environment work seamlessly on your system.

### System Requirements

- **Windows**: Windows 10 (Build 19041+) or Windows 11 with admin privileges
- **Linux**: Ubuntu 20.04 LTS or later
- **RAM**: Minimum 4GB (8GB+ recommended)
- **Disk Space**: At least 10GB free space
- **Internet**: Active internet connection for downloading Docker images

---

## Windows Setup

> **Linux users:** Skip this entire section. Jump straight to [Linux / WSL Setup](#linux--wsl-setup) → Step 6.

### Step 1: Install Docker Desktop for Windows

#### 1.1 Download Docker Desktop

1. Navigate to the official Docker installation page:
   ```
   https://docs.docker.com/desktop/setup/install/windows-install/
   ```

2. You should see a page similar to this:

   ![Docker Download Page](images/image1.jpeg)

3. Click on **"Docker Desktop for Windows - x86_64"** to download the installer.

4. Wait for the download to complete.

#### 1.2 Install Docker Desktop

1. Locate the downloaded `Docker Desktop Installer.exe` file and double-click to run it.

2. The installation wizard will start. Keep all **default options** selected throughout the installation process.

3. When prompted to install WSL (Windows Subsystem for Linux), **allow all incoming requests** and proceed with the installation. Docker will automatically configure WSL for you.

4. Once installation is complete, your screen should look similar to this:

   ![Docker Desktop Application](images/image2.jpeg)

5. Docker Desktop is now installed and running on your system.

### Step 2: Verify Docker Installation

#### 2.1 Check Docker is Running

1. Open **PowerShell** or **Command Prompt** as Administrator.

2. Type the following command to check your WSL installation:
   ```powershell
   wsl -l -v
   ```

3. You should see output similar to this:

   ![WSL List Verification](images/image3.jpeg)

4. **Verify**: Look for an entry labeled `docker-desktop` in the output. If present, Docker is successfully installed and WSL is properly configured.

### Step 3: Install Windows Subsystem for Linux (WSL) with Ubuntu 22.04

#### 3.1 What is WSL?

**Windows Subsystem for Linux (WSL)** is a compatibility layer that allows you to run Linux distributions natively on Windows. It provides a Linux kernel and environment without the overhead of a traditional virtual machine.

**Why do we need WSL?**
- The ROS 2 simulation environment is Linux-based; WSL brings Linux to Windows
- Provides better performance than traditional virtual machines
- Seamless integration with Windows and Docker

#### 3.2 Install Ubuntu 22.04

1. Open **PowerShell** as Administrator.

2. Type the following command:
   ```powershell
   wsl --install -d Ubuntu-22.04
   ```

3. The system will begin downloading and installing Ubuntu 22.04. This may take several minutes.

4. Once complete, you'll be prompted to create a username and password. **Set these credentials** — you'll use them to log into your Linux environment.

5. After setup, your terminal should display your Linux username and you'll be logged into the Ubuntu environment:

   ![WSL Terminal Login](images/image4.jpeg)

#### 3.3 Accessing WSL Anytime

To access your WSL environment from Windows PowerShell or Command Prompt at any time, simply type:
```powershell
wsl
```

---

## Linux / WSL Setup

### Step 4: Configure Docker Desktop for WSL Integration

> **Linux users (not using WSL):** If you installed Docker Engine directly on Linux, skip Steps 4 and 5 and go to [Step 6](#step-6-clone-repository-and-build-container).

#### 4.1 Open Docker Settings

1. Open the **Docker Desktop** application.

2. Click on the **Settings** icon (gear icon) in the top-right corner:

   ![Docker Settings](images/image5.jpeg)

#### 4.2 Enable WSL Integration

1. In the Settings menu, navigate to **Resources** → **WSL Integration**.

2. You should see a screen similar to this:

   ![WSL Integration Settings](images/image6.jpeg)

3. **Enable** the toggle for `Ubuntu-22.04`.

4. Click **Apply and Restart** to apply the changes and restart the Docker daemon.

5. Wait for Docker to fully restart (this may take a few minutes).

### Step 5: Verify Docker in WSL

#### 5.1 Access WSL and Test Docker

1. Open **PowerShell** or **Command Prompt**.

2. Type the following command to enter your WSL Ubuntu environment:
   ```powershell
   wsl
   ```

3. Once inside the Linux environment, type:
   ```bash
   docker
   ```

4. If Docker is properly set up, you'll see Docker's help output. If you see an error, wait a moment for Docker to fully initialize and try again.

### Step 6: Clone Repository and Build Container

#### 6.1 Create a Working Directory

Inside your WSL / Linux environment, create a directory for your project:
```bash
mkdir ~/workspace
cd ~/workspace
```

#### 6.2 Clone the Repository

Clone the repository:
```bash
git clone https://github.com/purvanyaTyagi/Simulation-Development-Recruitment-Hackathon .
```

#### 6.3 Build and Start the Container

> ⚠️ **Windows Users — Important:** Make sure `docker compose up` is run from within your **WSL Ubuntu 22.04 environment**, not from native Windows PowerShell or Command Prompt. Running it outside of WSL may cause the container to fail or behave unexpectedly. Once the container is running, you can attach to it from any terminal (WSL or otherwise) using `docker exec`.

From the repository root directory, build and start the Docker container:

```bash
docker compose build
docker compose up
```

Or do both in one step:

```bash
docker compose up --build
```

This will:
- Download the necessary Docker images (first time only)
- Build the container with all ROS 2 dependencies installed
- Start the container

The build process may take **10–15 minutes** depending on your internet speed and system performance.

#### 6.4 Verify Successful Build

Once the container is built and running, you should see output similar to this:

   ![Docker Compose Successful Build](images/image7.jpeg)

The container is now running in the background, and you can open a new terminal to interact with it.

---

## Running the Simulation

### Step 7: Access the Running Container and Launch RViz2

#### 7.1 Open a New Terminal

Open a **new terminal window** (keep the previous one running the Docker container).

#### 7.2 Enter the Docker Container

Type the following command to access the running container:
```bash
docker exec -it ros2-humble-ignition bash
```

This command:
- `docker exec`: Executes a command in a running container
- `-it`: Allows interactive terminal access
- `ros2-humble-ignition`: The name of your running container
- `bash`: Opens a bash shell inside the container

You'll now be inside the Docker container with access to the full ROS 2 environment.

> **Note:** Once the container is running (started via WSL), you can attach to it from **any terminal** — including native Windows PowerShell or Command Prompt — using the `docker exec -it ros2-humble-ignition bash` command above.

#### 7.3 Launch the Simulation

Inside the container, type:
```bash
ros2 launch robot_sim sim.launch.py
```

**RViz2 should now launch on your screen!** You'll see the robot visualiser with your robot model rendered based on your URDF file and moving according to the logic in `trajectory.cpp`.

---

## Troubleshooting

### Docker-Related Issues

**Problem**: Docker command not found in WSL
- **Solution**: Ensure Docker Desktop is running on Windows and WSL integration is enabled in Settings → Resources → WSL Integration

**Problem**: Container fails to start
- **Solution**:
  1. Check Docker Desktop logs (Settings → Troubleshoot)
  2. Ensure sufficient disk space (at least 10GB free)
  3. Restart Docker Desktop

**Problem**: Out of memory errors
- **Solution**: Increase Docker's memory allocation in Settings → Resources → Memory (set to at least 4GB)

### WSL-Related Issues

**Problem**: WSL command not found
- **Solution**: Ensure Windows 10 (Build 19041+) or Windows 11. Update Windows if necessary.

**Problem**: Cannot enter WSL environment
- **Solution**:
  1. Run PowerShell as Administrator
  2. Check WSL status: `wsl --status`
  3. Reinstall if necessary: `wsl --install -d Ubuntu-22.04`

### RViz2 / Simulation Issues

**Problem**: RViz2 doesn't launch or appears blank
- **Solution**:
  1. Ensure you're inside the Docker container (verify with `whoami` command)
  2. Check graphics drivers are up to date on your host system
  3. Try setting the display: `export DISPLAY=:0` inside the container, then relaunch

**Problem**: Performance is slow
- **Solution**:
  1. Increase Docker's CPU allocation
  2. Close unnecessary applications on your host machine
  3. Reduce the complexity of your URDF model if needed

---

## Additional Resources

- **ROS 2 Documentation**: https://docs.ros.org
- **RViz2 User Guide**: https://docs.ros.org/en/humble/Tutorials/Intermediate/RViz/RViz-Main.html
- **URDF Tutorials**: https://docs.ros.org/en/humble/Tutorials/Intermediate/URDF/URDF-Main.html
- **Docker Documentation**: https://docs.docker.com

---

## Support

If you encounter issues not covered in this guide:
1. Check the official ROS 2 documentation
2. Review Docker logs and error messages carefully
3. Consult your instructor or team lead

---
