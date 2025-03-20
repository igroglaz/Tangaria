Dockerfiles
-
Build container:

for build x.y.z (Build t) (https://github.com/draconisPW/PWMAngband/releases/tag/x.y.z-build-t)

`docker build -f ./Dockerfile-x.y.z-build-t -t pwmangband:x.y.z-build-t .`

for build current git (https://github.com/draconisPW/PWMAngband)

`docker build -f ./Dockerfile.git -t pwmangband:current-git .`

Create mount directory:

`mkdir ./pwmangband`

Add required permissions:

`chown -R 1000:1000 ./pwmangband`

Run container:

for x.y.z (Build t):

`docker run --name pwmangband -p 18346:18346 -v $(pwd)/pwmangband:/pwmangband -d pwmangband:x.y.z-build-t`

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
