# Hosts3D 1.17

## Highlights
- Packet rendering now distinguishes control traffic from payload-carrying traffic by shape:
  - cube for control / no payload
  - double cuboid for payload
  - triple cuboid for larger payload
- TCP control traffic is now distinguished more clearly:
  - orange cube for setup / finish traffic
  - red cube for TCP reset traffic
- Name and discovery traffic such as DNS, mDNS, LLMNR, and NetBIOS name service is highlighted with spherical packet markers.
- ICMP and fragmented traffic is highlighted with pyramid packet markers.
- The OSD packet legend now shows miniature angled 3D packet examples instead of flat text markers, and clicking those examples applies the matching protocol, packet-form, or TCP-control filter.
- Packet traffic filters are now exclusive across sensor, protocol, port, packet form, and TCP control, and the currently active OSD example row is highlighted directly in the legend.
- The `Packets` menu now mirrors the active packet-filter state for sensor, protocol, form, port, and TCP control.
- Selected host details now include the last observed protocol, the last important port, and the last observed discovery name when known.
- New packet recordings preserve the extended packet-shape metadata, while older `traffic.hpt` recordings remain readable.

## Notes
- Packet colours still indicate protocol family.
- Packet shape now adds an additional visual layer for behavior and traffic type.
- The new OSD packet examples use the same OpenGL packet objects as the live animated packets, so the legend stays consistent with the scene.
