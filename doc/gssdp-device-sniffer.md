---
Title: Manpage for gssdp-device-sniffer
---

# NAME

gssdp-device-sniffer - display SSDP packets on your network

# SYNOPSIS

**gssdp-device-sniffer** [**-i**] [**-6**]

# DESCRIPTION

gssdp-device-sniffer is a tool that will listen for SSDP announcements that happen
on your network.

In addition to recording the SSDP packets and providing raw display, it will also
keep track of the devices it has seen.

# OPTIONS

**-h**, **--help**
:     Display help

**-i**, **--interface**
:     Name of the network interface the sniffer will listen on

**-6**, **--prefer-v6**
:     Listen on IPv6 for SSDP announcements
