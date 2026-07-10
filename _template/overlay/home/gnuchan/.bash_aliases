# System
alias update='sudo apt update'
alias upgrade='sudo apt upgrade'
alias install='sudo apt install'
alias remove='sudo apt remove'
alias search='apt search'
alias cleanup='sudo apt autoremove && sudo apt autoclean'
alias q='exit'

# Files
alias ll='ls -lh'
alias la='ls -lha'
alias tree='find . -maxdepth 2 -type d | sort'

# System Info
alias ram='free -h'
alias disk='df -h'
alias cpu='cat /proc/cpuinfo | grep "model name" | head -1'
alias temp='sensors'
alias ip='ip -c a'
alias ps='ps aux --sort=-%mem'

# QTile
alias restart-qtile='qtile cmd-obj -o cmd -f restart'
alias reload-qtile='qtile cmd-obj -o cmd -f reload_config'

# X
alias xrestart='startx'