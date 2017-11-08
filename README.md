# VR Teleport

This plugin provides an easy way to add teleportation capabilities to any pawn in the context of VR.

The plugin introduces a new component (`VRTeleportComponent`) to the engine, which automatically takes care of a number of things involved in teleporting a player in VR, including:
* ray-casting and hit-testing (in line or projectile mode).
* avoiding blocking the ray cast with its owning actor.
* checking for the hit point being orthogonal with the ground.
* showing and hiding a marker of the teleport location.
* accounting for camera offset from the VR bounds when teleporting to a new target location.
* providing a default animation for teleportation (fade out and translate).

The component also handles replication / authority for multiplayer scenarios and has a number of events which can be implemented in either C++ or Blueprints to further customize its behavior.

## Installation

1. Download the source and place the whole folder in the **Plugins** folder of either the engine or your project.
2. Rebuild your C++ project so that the plugin is compiled for your architecture.

## Usage

you can find a quick start with images [here][l4].


[LinkedIn][l1] | [0xfede.io][l2] | [GitHub][l3]

[l1]: https://www.linkedin.com/in/federicosaldarini
[l2]: http://0xfede.io
[l3]: https://github.com/saldavonschwartz
[l4]: http://0xfede.makina.us/index.php/project/teleport-component
