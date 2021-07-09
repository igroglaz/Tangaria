Simple music system for PWMAngband
----------------------------------

* you can use mp3 or ogg files as background music while playing
* music files are picked and played from a directory depending on the
  character location; music is played continuously by chosing a file randomly
  from that directory, playing it once and looping the process until
  character location is changed
* by default, music is read from the /lib/music directory
* to play specific music in towns, put the music files in the
  /lib/music/generic-town subdirectory
* to play specific music in a specific town, put the music files in the
  /lib/music/shortname subdirectory, where 'shortname' is the short name of the
  town as set in /lib/gamedata/town.txt
* to play specific music in dungeons, put the music files in the
  /lib/music/generic-dungeon subdirectory
* to play specific music in a specific dungeon, put the music files in the
  /lib/music/shortname subdirectory, where 'shortname' is the short name of the
  dungeon as set in /lib/gamedata/dungeon.txt