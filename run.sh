#!/bin/sh
xhost +SI:localuser:root
sudo ./macro
xhost -SI:localuser:root  # Revoke access afterward