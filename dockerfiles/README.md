Build container:
  for build 1.6.1 (Build 1) (https://github.com/draconisPW/PWMAngband/releases/tag/1.6.1-build-1)
    `docker build -f ./Dockerfile-1.6.1-build-1 -t pwmangband:1.6.1-build-1 .`
  for build current git (https://github.com/draconisPW/PWMAngband)
    `docker build -f ./Dockerfile.git -t pwmangband:current-git .`
Create mount directory:
  `mkdir ./pwmangband`
Add required permissions:
  `chown -R 1000:1000 ./pwmangband`
Run container:
  for 1.6.1 (Build 1):
    `docker run --name pwmangband -p 18346:18346 -v $(pwd)/pwmangband:/pwmangband -d pwmangband:1.6.1-build-1`
  for current git:
    `docker run --name pwmangband -p 18346:18346 -v $(pwd)/pwmangband:/pwmangband -d pwmangband:current-git`
Check logs:
  `docker logs pwmangband`
After first startup in ./pwmangband will created PWMAngband install directories and files:
  /pwmangband/etc
  /pwmangband/games
  /pwmangband/share
  /pwmangband/private_dir (which will symlink to user files ~/.pwmangband/PWMAngband)
  /pwmangband/mangband.cfg (pwmangband server config)
