# Physics notes

These are exploratory design notes, not a committed simulator choice.

## World simulation and robot physics

Yard should own the persistent, changing yard environment. A robotics physics
engine could handle robot dynamics, actuators, and physical contact, while Yard
handles growth, mowing, weed removal, mulch disturbance, leaf accumulation, and
other environment processes.

Keep three connected representations of the world:

- **Yard state:** grass height and density, weeds, leaf coverage, moisture, and
  persistent changes caused by robot work.
- **Physical representation:** terrain, obstacles, robot joints and motors, and
  interactive objects, with detail appropriate to the task.
- **Visual and sensor representation:** vegetation, materials, lighting, and
  sensor-visible geometry derived from the same yard state.

For example, mowing could reduce grass height, generate clippings, and impose a
cutting load based on vegetation density. The renderer displays the changed
grass. This does not initially require individually simulated grass blades.

Optional photorealistic presentation and robot sensor fidelity are separate
settings. Reducing spectator rendering quality should not silently change robot
observations. A simulation without a visible window may still render cameras.

## Candidate simulators

- **MuJoCo:** an embeddable physics library focused on articulated bodies,
  actuators, and contact. A candidate when we want to own the engine architecture
  and build Yard's world simulation and presentation around robot physics.
- **Isaac Sim:** an integrated robotics platform with rendering, sensors,
  synthetic data tools, and ROS 2 integration. Worth evaluating if realistic
  camera-based autonomy is central; RTX sensors require an NVIDIA RTX GPU.
- **Gazebo:** a robotics simulator combining physics, rendering, sensor models,
  and plugins. Worth evaluating when testing a conventional robotics software
  stack is the main priority.

MuJoCo is not inherently slower than traditional game physics. Its specialization
can make robot dynamics efficient. Any performance advantage from a hybrid
approach must be measured; likely savings come from reduced detail and update
frequency for distant objects, not simply the choice of engine.

## Physics LOD around the robot

One possible architecture is a detailed MuJoCo simulation around the robot's
immediate vicinity, with cheaper simulation elsewhere. Distant objects could use
traditional game physics, reduced-order models, or procedural animation.

A garden gate swaying in distant wind might only need a simple wind-driven hinge
model. A shed door near the robot needs a physical hinge and contact response so
the robot can push it.

### Example: shed door handoff

1. At a distance, a cheap model updates the door's hinge angle and angular
   velocity.
2. Before contact is possible, add its MuJoCo representation: hinge, mass,
   inertia, limits, and relevant frame and surrounding collision geometry.
   Transfer its current angle and angular velocity.
3. While nearby, MuJoCo owns the door's motion. Apply wind as torque and let the
   renderer consume the resulting pose.
4. Once the robot leaves and no interaction requires the detailed model,
   transfer the resulting state back to the cheap model.

### Ownership and boundary rules

- **One physics owner per dynamic object.** Multiple representations may exist,
  but only one solver determines its motion at a time.
- **Keep interacting bodies in the same solver.** Promote complete relevant
  assemblies and contact partners. If a robot pushes a wheelbarrow into the shed
  door, the wheelbarrow also belongs in that MuJoCo simulation.
- **Use a buffer and hysteresis.** Promote before contact is possible and demote
  at a greater distance to avoid repeated switching near the boundary.
- **Let interactions extend the region.** Held, touching, or connected objects
  remain active even outside the nominal radius. Distance alone is insufficient.
- **Preserve state at handoff.** Transfer pose and velocity, including joint
  state. Different friction, damping, and constraint models can still cause
  acceleration discontinuities, so compatible parameters matter.
- **Keep sensor visibility independent of physics ownership.** A distant gate
  must still appear at its current pose in camera and lidar scenes even when
  MuJoCo does not simulate it.
- **Coordinate time and state publication.** Perform ownership changes at
  defined simulation boundaries so an object is neither advanced twice nor
  omitted during a handoff.

Avoid solving a robot in one engine and a door it is pushing in another, then
trying to exchange contact forces between them. That introduces coupled-solver
complexity beyond straightforward ownership transfer.

## MuJoCo model editing considerations

MuJoCo supports programmatic model specification editing, including adding,
removing, and attaching model subtrees. Structural changes such as adding bodies
and joints require compilation of the resulting model specification.

Do not assume this is a cheap per-frame spawn operation. Measure compilation,
state transfer, and any resulting simulation or rendering stalls before choosing
how frequently to change the local model. Preserve stable Yard object identities
across model changes rather than relying on simulator indices remaining stable.

## Initial prototype and open questions

Start with one wheeled mower, uneven ground, a concrete edge, a fence, and a grass
patch that can be cut. Exercise the complete loop:

Motor commands → motion and contact → sensors → controller → persistent yard
changes.

Initially keep the robot and a modest set of interactive objects permanently in
MuJoCo. Keep vegetation growth, distant animation, and other yard processes in
Yard. Design for ownership transfer, but introduce dynamic migration only when
profiling justifies it.

Questions to resolve through prototypes:

- Is the main priority robot dynamics and interaction, realistic perception,
  or both?
- How much physical detail do actual yard tasks require?
- Does a simplified full-yard collision model already meet the performance goal?
- What are the costs and visible effects of model compilation and state handoff?
- How large should promotion buffers be for robot and object motion?
- If multiple robots are introduced, how will their interacting regions share
  one solver when they meet?

## References

- [MuJoCo overview](https://mujoco.readthedocs.io/en/stable/overview.html)
- [MuJoCo model editing](https://mujoco.readthedocs.io/en/stable/programming/modeledit.html)
- [Isaac Sim overview](https://docs.isaacsim.omniverse.nvidia.com/latest/index.html)
- [Isaac Sim RTX sensors](https://docs.isaacsim.omniverse.nvidia.com/latest/sensors/isaacsim_sensors_rtx.html)
- [Gazebo Sim](https://gazebosim.org/libs/sim/)

The hybrid ownership and physics LOD approach above is a proposed Yard design,
not an out-of-the-box feature promised by these simulators.
