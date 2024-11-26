#!/bin/bash

# Variables
SOURCE_FILE="cligpt.cpp"
EXECUTABLE_NAME="cligpt"
INSTALL_DIR="/usr/local/bin"
CONFIG_DIR="$HOME/.cligpt"
ENV_FILE="$CONFIG_DIR/.env"

# Step 1: Ensure prerequisites are installed
echo "Checking prerequisites..."
if ! command -v g++ &>/dev/null; then
    echo "Error: g++ is not installed. Install it and try again."
    exit 1
fi

if ! command -v curl-config &>/dev/null; then
    echo "Error: libcurl is not installed. Install it and try again."
    exit 1
fi

# Step 2: Build the project
echo "Building the project..."
g++ $SOURCE_FILE -o $EXECUTABLE_NAME -lcurl -Iinclude || { echo "Build failed. Exiting."; exit 1; }

# Step 3: Move the executable to the installation directory
echo "Installing the executable..."
sudo mv $EXECUTABLE_NAME $INSTALL_DIR || { echo "Failed to install the executable. Exiting."; exit 1; }

# Step 4: Setup the configuration directory and .env file
echo "Setting up configuration directory..."
mkdir -p $CONFIG_DIR

if [ ! -f $ENV_FILE ]; then
    echo "Creating default .env file..."
    cat > $ENV_FILE <<EOL
GPT_KEY=
GPT_NAME=ChatGPT
PERSONALITY=Friendly AI
USER_NAME=User
EOL
else
    echo ".env file already exists. Skipping creation."
fi

# Completion message
echo "Installation complete! You can now run the app using the '$EXECUTABLE_NAME' command."