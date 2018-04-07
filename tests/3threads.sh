#!/bin/bash

seq 0 9 | nc localhost 33333 & seq 10 19 | nc localhost 33333 & seq 20 29 | nc localhost 33333