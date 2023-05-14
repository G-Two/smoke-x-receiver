#!/bin/bash

if [ $# -eq 1 ]
then
  TGT_DIR=$1
else
  TGT_DIR=.
fi

npm install --prefix $TGT_DIR
npm run --prefix $TGT_DIR build
find $TGT_DIR/dist -type f -name '*.js' -delete
find $TGT_DIR/dist -type f -name '*.css' -delete
find $TGT_DIR/dist -type f -name '*.ico' -delete
find $TGT_DIR/dist -type f -name '*.html' -delete
find $TGT_DIR/dist -type f -name '*.png' -delete
find $TGT_DIR/dist -type f -name 'mockServiceWorker.*' -delete
