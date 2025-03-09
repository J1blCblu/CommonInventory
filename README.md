# Common Inventory
A **work-in-progress** inventory plugin for **Unreal Engine** with a consistent transactional network model.
Unlike most existing implementations, the system targets consistency, reliability with the robust engine integration.

At the current stage, the plugin provides the core functionality for implementing gameplay systems:
1. Extendable `UCommonInventoryRegistry` for maintaining archetypes.
2. Optional data source for gathering data from `UAssetManager`.
3. Core data types: `FCommonItem`, `FCommonItemStack`, `UCommonItemDefinition`.

Next global milestones:
1. `UCommonInventoryComponent` implementation.
2. Network support through the command controller.
3. Various utilities for fully utilizing the system.
4. Various tests for validating the functionality.

# Dependencies
* [**Variadic Struct**](https://github.com/J1blCblu/VariadicStruct).
