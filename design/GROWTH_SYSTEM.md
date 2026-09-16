# Botanical growth and roots — research notes

Initial research: 2026-09-16. **Exploratory notes only:** no growth system, root
simulation, species calibration or new dependency is implemented or selected here.
The objective is recognizable species developing plausibly through time, including
saplings and their response to other plants. This is a starting reading list and
a set of design questions, not a comprehensive botanical model. This initial pass
uses research abstracts and USFS species accounts; detailed equation review and
woody-species experimental calibration remain future research.

## Starting point in Yard

The tree-gen port generates static branch skeletons and foliage, then derives
meshes. Its taper, flare, tropism and branch parameters describe procedural form;
they are not a persistent developmental history. There are no root organs, active
buds, age-dependent resource budgets or growth responses to neighboring plants.
Changing a mature preset's scale does not establish correct sapling proportions.
See [the current port](TREES.md) and [future tasks](../TODO.md#trees).

Reuse curve/sweep geometry where useful, but separate a future botanical organ
graph from its render tessellation. Roots can share curve and branching machinery
with shoots without inheriting shoot placement or developmental rules. Existing
overlapping branch meshes are also not a solution for smooth root-collar joins.

## Research observations and what they suggest

The observations below are sourced; the proposed Yard applications are design
inferences, not claims that these sources validate a Yard implementation.

| Source / observation | Proposed use and boundary |
| --- | --- |
| Palubicki et al. (2009) generate trees and shrubs through a self-organizing model of bud/branch competition for light or space and internal regulation. | Candidate architecture for local bud decisions producing whole-crown form; visual plausibility alone does not establish physiological or species accuracy. [Paper and abstract](https://algorithmicbotany.org/papers/selforg.sig2009.html). |
| Runions et al. (2007) describe space colonization for controllable tree structure. | Useful geometric comparator or placement tool; do not treat attraction points as a carbon budget or a complete botanical growth law. [Paper and abstract](https://algorithmicbotany.org/papers/colonization.egwnp2007.html). |
| Schnepf et al. (2018), CRootBox, model root architecture interacting with soil, including heterogeneous environments and coupling to water flow. | Study root types, soil queries and model separation. This is a candidate reference, not an adopted library or evidence that its available parameters cover mature woody trees. [Research abstract](https://pubmed.ncbi.nlm.nih.gov/29432520/). |
| Hayes et al. (2014) show that UV-B signaling can suppress neighbor-induced elongation in Arabidopsis. Their paper also describes red/far-red changes associated with nearby foliage. | Light is both an energy supply and a developmental signal. Do not transfer herbaceous experimental response magnitudes directly to trees; obtain woody-species evidence before parameterizing spectral responses. [Research abstract](https://pubmed.ncbi.nlm.nih.gov/25071218/), [full paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC4136589/). |

These suggest investigating a **functional–structural plant model**: architecture
controls access to resources, resource acquisition constrains new growth, and that
growth changes the architecture. The intended scope is whole-plant and organ-level
behavior, with empirically fitted response curves where possible. Molecular
signaling need not be simulated explicitly to represent its measured effects.

## Species identity before parameter tuning

A proposed species profile should record a botanical name and taxonomic authority,
provenance/cultivar where known, source references and measurement conditions.
Distinguish species, genotype/individual variation, age/developmental stage and
site conditions. Store units, distributions or ranges, and uncertainty; avoid
inventing a universal growth rate from a mature-height entry.

Profile groups to research:

- Architecture: bud arrangement, apical control, branching angles/orders, shoot
  flush patterns, internode lengths, leaf/needle arrangement and crown development.
- Development: seedling establishment, juvenile foliage, sapling-to-adult changes,
  reproductive maturity, longevity, and responses to pruning or loss of the leader.
- Physiology: light-response curves, maintenance costs, allocation and reserves,
  water/nutrient limitations, shade tolerance and shade-avoidance responses by stage.
- Phenology: budburst, leaf expansion, dormancy, senescence and leaf retention;
  investigate temperature, chilling, day-length and water-stress controls by species.
- Roots: structural/absorptive roles, branching and depth distributions, thickening,
  turnover, environmental plasticity, and vegetative regeneration where supported.

Small illustrative research contrasts, not a committed species roster:

| Taxon | Source-backed distinction to preserve |
| --- | --- |
| Quaking aspen (*Populus tremuloides*) | Very shade intolerant; root suckering matters to regeneration. Its root architecture depends on soil conditions. A future clone can require identity beyond one visible stem. [USFS Silvics](https://research.fs.usda.gov/silvics/quaking-aspen). |
| Loblolly pine (*Pinus taeda*) | Shade tolerance changes with age. Rooting varies with age, soil texture, drainage and restricting layers; lateral spread can exceed crown spread. This argues against fixed root-depth or crown-footprint rules. [USFS Silvics](https://research.fs.usda.gov/silvics/loblolly-pine). |
| Longleaf pine (*Pinus palustris*) | The seedling grass stage and release from competition are central to later height development. A generic age-to-height scale would miss that trajectory. [USFS Silvics](https://research.fs.usda.gov/silvics/longleaf-pine). |

Audit all imported preset labels; retain the original presets for compatibility
without implying every label is a resolved taxon or a validated local planting.
Select a small set of contrasting, site-appropriate species for initial calibration,
including a shade-tolerant comparison still to be researched. The current demo
aspen is a visual test subject, not evidence of suitability at the simulated site.

## Neighbor foliage: growth amount versus growth direction

Proposed environmental inputs should separate three questions:

1. **How much usable light arrives?** Estimate incident/absorbed photosynthetically
   active radiation over time, including direct and diffuse light, leaf area,
   orientation, transmission, self-shading and foliage from every plant type.
   Record units (for example photon flux per area per second); integrate a day
   rather than treating one camera frame as the plant's light supply.
2. **Where does it arrive from?** A directional light field can inform new shoot
   orientation and leaf placement, together with species architecture and gravity.
   Responses should act on developing organs; do not swivel an established woody
   trunk to chase the brightest instantaneous ray.
3. **What does the spectrum signal?** Investigate red/far-red and blue-light cues
   separately from total available energy. Spectral shade avoidance, directional
   phototropism and shade tolerance are different response dimensions. The Hayes
   study above motivates this distinction; quantitative tree responses remain an
   explicit research gap.

Proposed budget accounting must keep elongation separate from biomass gain: a
shade response may allocate limited resources differently, rather than create
extra growth from less energy. Account for respiration, reserves, water and
nutrients before allocating new tissue. Low light should permit suppression,
branch loss or death where species data supports those outcomes, not force every
plant to elongate successfully. Research response times and reversibility.

Other trees, shrubs, grass, weeds and vines should contribute biological leaf area
and seasonal occupancy even when rendered as impostors or a grass volume. A gap
opened by pruning/removal should change subsequent light exposure and development.
Mixed canopies need both competition within a plant and competition between plants.
Root competition for water and nutrients must remain distinct from foliage shading.

Do not use display RGB, camera exposure, the art-directed ambient fill or the
current opaque-leaf shadow map as calibrated plant-light measurements. Reuse spatial
acceleration where appropriate, but investigate a separate simulation light field
with leaf transmission and temporal integration. Changing SSAA, camera position,
frustum culling or grass LOD must not change biological growth.

## Realistic roots, including exposed roots

Proposed root state should distinguish the root collar, woody supporting axes,
branching laterals and absorptive roots. Keep underground continuation even when
only a few exposed roots are visible. A surface-only star of decorative tubes
would not meet the botanical objective.

Use species and soil conditions to constrain root initiation, direction, branching,
extension, secondary thickening and turnover. Investigate gravity, moisture,
aeration, nutrients, compaction, bedrock and barriers as separate influences;
CRootBox is a starting point for the soil/model interface. Do not prescribe a
universal taproot or a mirrored copy of the crown.

Visible exposure should be an intersection between root geometry and the actual
soil surface, potentially changing with erosion, soil movement or root thickening.
Research species-appropriate buttressing and collar shape before adding them to
all taxa. A first visual prototype could use tapered sweeps; smooth collar and
root junctions, bark continuity and robust mesh generation need separate work.

Yard's present 64 cm density slab is not a sufficient general root-domain model.
Choose deeper soil layers or an independent root/soil field explicitly rather than
silently clipping roots to the visible terrain representation. Mechanical anchorage,
root contact and terrain deformation would need additional physical models; geometry
alone does not provide those behaviors. No physics backend is selected here.

## Persistent growth: candidate state and update loop

A proposed plant record would own stable IDs, species/profile version, origin
(seed, transplant or vegetative regeneration), age and developmental stage,
organ topology, bud states, foliage cohorts, living/dead tissue, reserves and
resource/stress history. A clonal species may also need a shared clone record.
Derived meshes and contact proxies should not become the authoritative growth state.

A candidate update sequence is to integrate environmental exposure, compute
resource gain/loss, allocate available resources, update buds/shoots/roots and
thickening, then process seasonal turnover and damage. All plants should read a
consistent environment snapshot before changes are committed, avoiding an advantage
for whichever plant happens to update first. Investigate adaptive time intervals:
weather/light sampling, organ extension and seasonal events need different scales.

Use the simulation calendar and reproducible weather inputs. Keep persistent growth
independent of rendering cadence and deterministic across save/load. Scrubbing the
current demo clock backward would require an explicit snapshot/replay policy; a
negative time step cannot undo biological development or mowing.

Saplings should emerge through this system with juvenile architecture and resource
history. Until that exists, a static juvenile preset can be labeled an approximation.
Do not regenerate a new random whole tree every year or infer full biological state
from a mature tree-gen mesh without a documented initialization approximation.

## Investigation order and eventual validation

These are candidate milestones, not an implementation commitment:

1. Assemble sourced species dossiers and age/size reference observations. Identify
   missing woody-plant light-response, root and phenology data before fitting.
2. Prototype a persistent organ graph and isolated juvenile growth with a resource
   ledger; separately explore collar/root mesh quality against photographs.
3. Compare identical initial plants in open light, one-sided shade and mixed canopy,
   then remove a neighbor. Track biomass, height, diameter, crown asymmetry, leaf
   area, bud activity, reserves and survival—not just a pleasing silhouette.
4. Add soil heterogeneity/root competition and seasonal/damage histories. Compare
   stage-specific rooting and regeneration against species observations.
5. Test step-size convergence, plant-update ordering, save/load, fixed seeds and
   render-LOD independence. Compare distributions across individuals and sites;
   reserve observations for validation rather than tuning to every example.

Open decisions include the first taxa and datasets, how much physiology to model,
light-field resolution/spectral bands, soil representation, how to initialize older
plants, and which simplified models retain the observed species differences.
Review candidate implementations and their licenses before any code reuse. The
papers above are research references, not new Yard dependencies.
