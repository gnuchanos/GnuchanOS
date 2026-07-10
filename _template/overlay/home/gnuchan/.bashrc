# ~/.bashrc — GnuchanOS custom prompt

# Color prompt
PS1='\[\e[0;35m\]\u\[\e[0;37m\]@\[\e[0;35m\]\h\[\e[0;37m\]:\[\e[0;36m\]\w\[\e[0;37m\]\$ '

# Aliases
if [ -f ~/.bash_aliases ]; then
    . ~/.bash_aliases
fi

# Editor
export EDITOR=nano

# Python
export PYTHONSTARTUP=~/.pythonrc 2>/dev/null || true