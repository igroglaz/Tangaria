#!/usr/bin/env bash

#Test mount volume and exists dirs
[ -d /pwmangband/etc ] && echo '/pwmangband/etc exist' || cp -R /install/etc /pwmangband/
[ -d /pwmangband/games ] && echo '/pwmangband/games exist' || cp -R /install/games /pwmangband/
[ -d /pwmangband/share ] && echo '/pwmangband/share exist' || cp -R /install/share /pwmangband/
[ -d /pwmangband/private_dir ] && echo '/pwmangband/private_dir exist' || mkdir -p /pwmangband/private_dir
echo 'Create symlink ~/.pwmangband/pwmangband'
mkdir -p ~/.pwmangband
ln -s /pwmangband/private_dir ~/.pwmangband/PWMAngband
[ -f /pwmangband/mangband.cfg ] && echo '/pwmangband/mangband.cfg exist' || cp /install/mangband.cfg /pwmangband/

cd /pwmangband
./games/pwmangband
