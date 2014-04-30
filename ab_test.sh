#!/bin/sh
ab -n 4000 -c 1000 -p post.txt 127.0.0.1:80/iheard
