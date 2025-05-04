#!/bin/bash

set -e  # Stop if any error occurs

sudo pacman -Sy lua lua-language-server ripgrep fd tree-sitter

echo "📁 Copying 'nvim' folder into ~/.config..."
cp -r nvim ~/.config/

echo "📦 Cloning packer.nvim..."
git clone --depth 1 https://github.com/wbthomason/packer.nvim \
  ~/.local/share/nvim/site/pack/packer/start/packer.nvim

echo "⚙️  Starting plugin installation..."

# Start Neovim in headless mode and run PackerSync
nvim --headless +PackerSync +qa


sudo pacman -Sy pyright 

echo "✅ All done. You can now open Neovim!"
