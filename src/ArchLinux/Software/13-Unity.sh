# cd ~/Unity/Hub/Editor/2021.3.45f2/Editor/Data
# mv bee_backend bee_backend_rea
# chmod a+x bee_backend


#! /bin/bash

args=("$@")
for ((i=0; i<"${#args[@]}"; ++i))
do
    case ${args[i]} in
        --stdin-canary)
            unset args[i];
            break;;
    esac
done
${0}_real "${args[@]}"


yay -Sy libxml2-legacy unityhub

sudo pacman -S dotnet-sdk dotnet-runtime mono
