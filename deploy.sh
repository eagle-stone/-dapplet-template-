#!/bin/bash
mkdir -p server_log
gunicorn -w 3 run_demo_server:app -b 0.0.0.0:8769 -t 120 \