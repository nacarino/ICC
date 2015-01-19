#!/bin/bash

awk '{ print $4 }' $1 | sort -n | tail -1
