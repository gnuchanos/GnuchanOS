#
# ~/.bashrc
#

[[ $- != *i* ]] && return

alias ls='ls --color=auto'
alias grep='grep --color=auto'
alias ll='ls -lah --color=auto'
alias df='df -h'
alias free='free -m'
alias c='clear'

PS1='\[\e[1;34m\][\[\e[1;32m\]\u\[\e[1;36m\]@\[\e[1;35m\]\h\[\e[1;34m\]]=\[\e[1;33m\]|\[\e[1;36m\] (\w)\n\[\e[1;31m\]:> \[\e[0m\]'

HISTCONTROL=ignoredups:erasedups
HISTSIZE=10000
HISTFILESIZE=20000
shopt -s histappend
export HISTTIMEFORMAT="%F %T "

export TERM=xterm-256color

stty -ixon

zsh

neofetch
complete -cf sudo
stty erase ^H
