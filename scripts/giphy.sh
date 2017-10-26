#!/bin/bash

search="$1"
curl "http://api.giphy.com/v1/gifs/translate?s=$search&api_key=$GIPHY_API_KEY" 2> /dev/null| python -c "import sys, json; print json.load(sys.stdin)['data']['url']"
