# M5 - ACL/NAT Rule Engine Case Study

## Objective

This module implements a simplified ACL/NAT rule engine from a network product engineering perspective. The engine reads configurable ACL, NAT, and route rules from a file, processes sample packet metadata, applies policy decisions, performs NAT translation, and records forwarding or drop decisions.

## Product Context

In a firewall, gateway, router, or network appliance, packets are not simply received and forwarded. A product software component must evaluate ACL policies, apply NAT translation, check routing decisions, and generate logs that explain whether the packet was forwarded or dropped.

## Build

```bash
make