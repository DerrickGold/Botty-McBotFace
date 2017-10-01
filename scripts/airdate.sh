#!/bin/bash

SCRIPTNAME=$(basename $0)
UNIT_REGEX="[1-9][0-9]?"
EPISODE_REGEX="$UNIT_REGEX-$UNIT_REGEX"
DATE_REGEX="[0-9]{2} +[A-za-z]{3} +[0-9]{2}"

caller="$1"
show=$(echo "$2" | sed 's/ //g')

if [ -z "$2" ]; then
    cat <<USAGE
 $caller: usage: ~script $SCRIPTNAME "<SHOW NAME>"
e.g. '~script $SCRIPTNAME "rick and morty"'
Uses 'http://epguides.com' to locate the next air date for
a given show.
USAGE
    
    exit 0
fi


url="http://epguides.com/$show/"
episodesList=$(curl "$url" 2> /dev/null | grep -E "$EPISODE_REGEX +$DATE_REGEX.*")
lastEpisode=$(echo "$episodesList" | tail -n 1)

epNum="S"$(echo "$lastEpisode" | grep -Eo "$EPISODE_REGEX" | sed 's/-/-E/')
date=$(echo "$lastEpisode" | grep -Eo "$DATE_REGEX")

echo "$caller: The next episode of $2 ($epNum) airs on $date"
