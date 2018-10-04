#!/bin/sh

$* | sed -e '/libibverbs|librdmacm/ d'
