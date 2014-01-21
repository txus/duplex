#!/bin/bash

rm -f /tmp/.duplex_remote /tmp/.duplex_replay /tmp/.duplex_pid;

nc -l 127.0.0.1 -p 20000 > /tmp/.duplex_remote &
nc -l 127.0.0.1 -p 20001 > /tmp/.duplex_replay &
bin/duplex -l 20005 -h 127.0.0.1 -p 20000 -H 127.0.0.1 -P 20001 > /tmp/.duplex_pid
pid=$(cat /tmp/.duplex_pid)

sleep 0.025
echo "Hello world" | nc 127.0.0.1 20005
sleep 0.025

remote=$(cat /tmp/.duplex_remote)
replay=$(cat /tmp/.duplex_replay)
expected="Hello world"

if [ "$remote" != "$expected" ]; then
  echo "Failed: Remote didn't receive \"$expected\". It received \"$remote\" instead."
  return -1;
fi

if [ "$replay" != "$expected" ]; then
  echo "Failed: Replay didn't receive \"$expected\". It received \"$replay\" instead."
  return -1;
fi

echo "All tests passed!"

echo "Cleaning up daemon..."
kill -9 $pid

rm -f /tmp/.duplex_remote /tmp/.duplex_replay /tmp/.duplex_pid;

return 0;