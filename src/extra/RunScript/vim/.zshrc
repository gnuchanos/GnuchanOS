autoload -Uz compinit
compinit

# Exit if this is not an interactive shell
[[ $- != *i* ]] && return

# Aliases
alias ls='ls --color=auto'
alias grep='grep --color=auto'
alias ll='ls -lah --color=auto'
alias df='df -h'
alias free='free -m'
alias c='clear'

# Fancy Zsh Prompt (same look as your Bash PS1)
PROMPT="%F{blue}[%F{green}%n%F{cyan}@%F{magenta}%m%F{blue}]=%F{yellow}|%F{cyan} (%~)%f
%F{red}:> %f"

# Better command history
HISTSIZE=10000
SAVEHIST=20000
HISTFILE=~/.zsh_history

setopt hist_ignore_dups
setopt hist_expire_dups_first
setopt append_history
setopt share_history

# Terminal color support
export TERM=xterm-256color

# Fix Ctrl+S / Ctrl+Q freeze issue
stty -ixon

# Run neofetch on shell startup
neofetch

# Sudo auto-completion (Zsh version of "complete -cf sudo")
zstyle ':completion:*:sudo:*' command-path /usr/bin /usr/sbin /bin /sbin

# Backspace fix
stty erase ^H

# Autosuggestions
source /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh

# Syntax Highlighting (MUST be last!)
source /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh

bindkey '\e[3~' delete-char

bindkey '\e[1;5C' forward-word   # Ctrl + Right
bindkey '\e[1;5D' backward-word  # Ctrl + Left
