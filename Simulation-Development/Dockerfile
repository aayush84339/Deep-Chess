ARG ROS_DISTRO=humble
FROM osrf/ros:humble-desktop

LABEL maintainer="Purvanya <mail2purvanya@gmail.com>"

SHELL ["/bin/bash", "-c"]

# ------------------------------------------------
# Base utilities
# ------------------------------------------------

RUN apt-get update && apt-get install -y \
    curl \
    git \
    wget \
    vim \
    nano \
    dbus-x11 \
    python3-pip \
    sudo \
    libyaml-cpp-dev \
    python3-colcon-common-extensions

# ------------------------------------------------
# Install Gazebo + ROS packages
# ------------------------------------------------

RUN apt-get update && apt-get install -y \
    ros-humble-ros-gz \
    ros-humble-ros2-control \
    ros-humble-ros2-controllers \
    ros-humble-ros-gz-sim \
    ros-humble-ros-gz-bridge \
    ros-humble-xacro \
    ros-humble-gz-ros2-control \
    ros-humble-joint-state-publisher \
    ros-humble-robot-state-publisher\
    gedit

# ------------------------------------------------
# Create non-root user
# ------------------------------------------------

ARG USERNAME=devuser
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd -m --uid $USER_UID --gid $USER_GID $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME

# ------------------------------------------------
# ROS environment
# ------------------------------------------------

RUN echo "source /opt/ros/${ROS_DISTRO}/setup.bash" >> ~/.bashrc

WORKDIR /home/devuser

# ------------------------------------------------
# Copy workspace
# ------------------------------------------------

COPY --chown=devuser:devuser workspace /home/devuser/workspace

WORKDIR /home/devuser/workspace

# ------------------------------------------------
# Build workspace
# ------------------------------------------------

RUN source /opt/ros/${ROS_DISTRO}/setup.bash && \
    colcon build

# ------------------------------------------------
# Source workspace automatically
# ------------------------------------------------

RUN echo "source /home/devuser/workspace/install/setup.bash" >> ~/.bashrc
ENV IGN_GAZEBO_SYSTEM_PLUGIN_PATH=/opt/ros/${ROS_DISTRO}/lib

CMD ["bash"]
