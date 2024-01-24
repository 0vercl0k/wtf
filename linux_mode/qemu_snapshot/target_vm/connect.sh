#!/bin/bash
ssh -i ./image/bookworm.id_rsa -p 10021 -o "StrictHostKeyChecking no" root@localhost
