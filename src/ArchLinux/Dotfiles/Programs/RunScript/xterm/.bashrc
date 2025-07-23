#
# ~/.bashrc
#

# Eğer interaktif bir oturum değilse çık
[[ $- != *i* ]] && return

# Renkli ls, grep, ve ekstra alias'lar
alias ls='ls --color=auto'
alias grep='grep --color=auto'
alias ll='ls -lah --color=auto'
alias df='df -h'
alias free='free -m'
alias c='clear'

# Şekilli Prompt
PS1='\[\e[1;34m\][\[\e[1;32m\]\u\[\e[1;36m\]@\[\e[1;35m\]\h\[\e[1;34m\]]=\[\e[1;33m\]|\[\e[1;36m\] (\w)\n\[\e[1;31m\]:> \[\e[0m\]'

# Daha iyi komut geçmişi
HISTCONTROL=ignoredups:erasedups
HISTSIZE=10000
HISTFILESIZE=20000
shopt -s histappend
export HISTTIMEFORMAT="%F %T "

# Terminal renk desteği
export TERM=xterm-256color

# Ctrl+S ve Ctrl+Q bug'larını kapat
stty -ixon


neofetch
fish




