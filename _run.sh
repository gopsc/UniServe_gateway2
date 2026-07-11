#!/bin/bash
cd "$(dirname "$0")" || exit
./build/gateway.out -i example.ini 1>/dev/null
