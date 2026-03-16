# WFC Pathfinding Constraint - Maze Solvability Feature

## Overview

The `UWFCPathfindingConstraint` ensures that generated mazes and dungeons are solvable by validating that a path exists between specified start and end locations. This feature works seamlessly with the existing RandomStream-based network replication system, ensuring deterministic generation across clients.

## How It Works

### Core Concept

The constraint uses A* pathfinding to verify that:
1. Tiles have at least N "passable" edges (default: 2)
2. A valid path exists from start to end using tiles with matching passable edges
3. Tiles that would block critical paths are banned during generation

### Tile Traversability

A tile is considered **traversable** if it has at least `MinPassableEdges` edges tagged with one of the `PassableEdgeTags`. For example:
- A 1¡Á1¡Á1 corridor tile with "Door" tags on opposite faces: **traversable** (2 passable edges)
- A 3¡Á3¡Á3 room with "Door" tags on 4 sides: **traversable** (4 passable edges)
- A dead-end with only 1 "Door" tag: **not traversable** by default (only 1 passable edge)

### Integration with RandomStream

**Key Feature**: The pathfinding constraint does NOT interfere with RandomStream-based tile selection!

- The constraint only **bans** tiles that would make the maze unsolvable
- It does NOT change the random selection process
- Network replication remains deterministic with ultra-low bandwidth
- All clients generate identical results using the same seed

## Configuration

### Basic Setup

1. Add the constraint to your WFC Generator
2. Configure the following properties:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
bool bEnablePathfinding = true;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
FGameplayTagContainer PassableEdgeTags;  // e.g., "WFC.EdgeType.Door", "WFC.EdgeType.Corridor"

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
FIntVector StartCellLocation = FIntVector(0, 0, 0);

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
FIntVector EndCellLocation = FIntVector(9, 9, 0);

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
int32 MinPassableEdges = 2;  // Minimum edges to be traversable
```

### Advanced Settings

```cpp
// How often to validate (every N cell selections)
// Lower = more frequent checks but slower generation
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
int32 ValidationFrequency = 10;

// Maximum A* iterations to prevent infinite loops
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
int32 MaxPathfindingIterations = 10000;
```

## Usage Examples

### Example 1: Simple 2D Maze

```cpp
// In your Blueprint or C++
UWFCGeneratorComponent* Generator = GetComponentByClass<UWFCGeneratorComponent>();

// Add the pathfinding constraint
UWFCPathfindingConstraint* PathConstraint = NewObject<UWFCPathfindingConstraint>(Generator);
PathConstraint->bEnablePathfinding = true;

// Define passable tags (must match your tile edge tags)
PathConstraint->PassableEdgeTags.AddTag(FGameplayTag::RequestGameplayTag("WFC.EdgeType.Door"));
PathConstraint->PassableEdgeTags.AddTag(FGameplayTag::RequestGameplayTag("WFC.EdgeType.Corridor"));

// Set start and end locations
PathConstraint->StartCellLocation = FIntVector(0, 0, 0);
PathConstraint->EndCellLocation = FIntVector(9, 9, 0);

// Set minimum passable edges (2 = corridor, 3+ = room/junction)
PathConstraint->MinPassableEdges = 2;

// Add to generator
Generator->GetGenerator()->GetConstraints().Add(PathConstraint);
```

### Example 2: 3D Dungeon with Multiple Floors

```cpp
UWFCPathfindingConstraint* PathConstraint = NewObject<UWFCPathfindingConstraint>(Generator);
PathConstraint->bEnablePathfinding = true;

// Define passable tags
PathConstraint->PassableEdgeTags.AddTag(FGameplayTag::RequestGameplayTag("WFC.EdgeType.Door"));
PathConstraint->PassableEdgeTags.AddTag(FGameplayTag::RequestGameplayTag("WFC.EdgeType.Stairs"));
PathConstraint->PassableEdgeTags.AddTag(FGameplayTag::RequestGameplayTag("WFC.EdgeType.Hallway"));

// Start: Ground floor entrance
PathConstraint->StartCellLocation = FIntVector(0, 0, 0);

// End: Top floor treasure room
PathConstraint->EndCellLocation = FIntVector(5, 5, 2);

// Allow tiles with at least 2 passable edges
PathConstraint->MinPassableEdges = 2;

// Validate less frequently for better performance
PathConstraint->ValidationFrequency = 20;
```

### Example 3: Multiplayer with Network Replication

```cpp
// Server setup
AWFCTestingActor* WFCTestingActor = ...; // Your testing actor
WFCTestingActor->bEnableMultiplayer = true;
WFCTestingActor->bServerOnlyGeneration = true;

// Add pathfinding constraint to the generator
UWFCPathfindingConstraint* PathConstraint = NewObject<UWFCPathfindingConstraint>(
    WFCTestingActor->WFCGenerator->GetGenerator()
);
PathConstraint->bEnablePathfinding = true;
PathConstraint->PassableEdgeTags.AddTag(FGameplayTag::RequestGameplayTag("WFC.EdgeType.Door"));
PathConstraint->StartCellLocation = FIntVector(0, 0, 0);
PathConstraint->EndCellLocation = FIntVector(10, 10, 0);

// Add to constraints
WFCTestingActor->WFCGenerator->GetGenerator()->GetConstraints().Add(PathConstraint);

// Generate with seed
WFCTestingActor->ServerStartGeneration(12345);

// Result: All clients will generate the SAME solvable maze using the same seed!
// The pathfinding constraint only bans unsolvable tiles, preserving determinism.
```

## Tile Setup Requirements

### Tagging Your Tiles

For tiles to work with pathfinding, they must have proper edge tags:

1. **Create Edge Type Tags** in your project:
   - `WFC.EdgeType.Door`
   - `WFC.EdgeType.Corridor`
   - `WFC.EdgeType.Wall` (not passable)
   - `WFC.EdgeType.Empty` (not passable)

2. **Assign Tags to Tile Edges**:
   - In your `UWFCTileAsset3D` or `UWFCTileAsset2D` assets
   - For each tile definition, set the `EdgeTypes` map
   - Example: A corridor tile should have "Door" tags on 2 opposite edges

3. **Mark Passable Tags in Constraint**:
   - Add all tags that allow passage to `PassableEdgeTags`
   - The constraint will only allow paths through matching edges

### Example Tile Configuration

**Corridor Tile (1¡Á1¡Á1)**:
```
EdgeTypes:
  +X: WFC.EdgeType.Door
  -X: WFC.EdgeType.Door
  +Y: WFC.EdgeType.Wall
  -Y: WFC.EdgeType.Wall
  +Z: WFC.EdgeType.Wall
  -Z: WFC.EdgeType.Wall
```
This tile is **traversable** (2 passable edges: +X and -X)

**Room Tile (3¡Á3¡Á3)**:
```
EdgeTypes:
  +X: WFC.EdgeType.Door
  -X: WFC.EdgeType.Door
  +Y: WFC.EdgeType.Door
  -Y: WFC.EdgeType.Door
  +Z: WFC.EdgeType.Empty
  -Z: WFC.EdgeType.Empty
```
This tile is **traversable** (4 passable edges: all horizontal)

**Dead-end Tile**:
```
EdgeTypes:
  +X: WFC.EdgeType.Door
  -X: WFC.EdgeType.Wall
  +Y: WFC.EdgeType.Wall
  -Y: WFC.EdgeType.Wall
  +Z: WFC.EdgeType.Wall
  -Z: WFC.EdgeType.Wall
```
This tile is **NOT traversable** by default (only 1 passable edge)
To allow dead-ends, set `MinPassableEdges = 1`

## Performance Considerations

### Validation Frequency

The `ValidationFrequency` parameter controls how often pathfinding runs:
- **Lower values (1-5)**: More frequent checks, slower generation, better guarantees
- **Higher values (20-50)**: Less frequent checks, faster generation, may miss some cases
- **Recommended**: 10-20 for most use cases

### Grid Size Impact

- **Small grids (10¡Á10)**: Very fast, no performance concerns
- **Medium grids (50¡Á50)**: Still fast, consider `ValidationFrequency = 20`
- **Large grids (100¡Á100+)**: Use `ValidationFrequency = 50` or higher

### A* Iteration Limit

The `MaxPathfindingIterations` prevents infinite loops:
- **Default (10000)**: Suitable for most grids up to 50¡Á50¡Á10
- **Large grids**: Increase to `50000` or more
- If limit is reached, pathfinding fails gracefully

## Troubleshooting

### "No valid path found" Warning

**Cause**: The constraint cannot establish a path with current tile selections.

**Solutions**:
1. Check that `PassableEdgeTags` includes all your passable edge types
2. Verify tiles have proper edge type tags
3. Ensure `MinPassableEdges` isn't set too high
4. Check that start/end locations are valid
5. Add more tiles with passable edges to your tile set

### "Invalid start/end cell location" Error

**Cause**: Specified cell location is outside grid bounds.

**Solutions**:
1. Verify grid dimensions match your start/end locations
2. Remember: locations are 0-indexed
3. For a 10¡Á10 grid, valid range is (0,0,0) to (9,9,0)

### Contradictions During Generation

**Cause**: Pathfinding constraint banned too many tiles, leaving no valid options.

**Solutions**:
1. Increase tile variety in your asset collection
2. Lower `MinPassableEdges` to allow more tiles
3. Reduce `ValidationFrequency` to ban less aggressively
4. Check for conflicts with other constraints

### Different Results on Different Clients (Multiplayer)

**Cause**: RandomStream or constraint configuration differs.

**Solutions**:
1. Ensure all clients use the same seed
2. Verify constraint parameters are identical on all clients
3. Check that tile assets and tags are the same
4. Enable `bUseMultiplayerSeeds` in `UWFCGeneratorComponent`

## Technical Details

### Algorithm

The constraint uses a modified A* pathfinding algorithm:

1. **Heuristic**: Manhattan distance for optimal performance
2. **Cost Function**: Uniform cost (all edges = 1.0)
3. **Open List**: Sorted by F-cost (G + H)
4. **Closed List**: Set-based for O(1) lookup

### Constraint Execution Order

The pathfinding constraint runs during the normal constraint phase:
1. Initial constraints apply (boundary, adjacency, etc.)
2. Cell is selected using RandomStream
3. Pathfinding validates every N selections
4. If path is invalid, non-traversable tiles are banned
5. WFC continues with remaining valid tiles

### Network Behavior

In multiplayer:
- Server generates with a specific seed
- Seed is replicated to clients
- All clients run the same constraints with the same seed
- Pathfinding constraint makes identical decisions on all machines
- Result: Perfect synchronization with minimal bandwidth

## FAQ

**Q: Does this affect network replication?**
A: No! The constraint only bans tiles during generation. The RandomStream logic remains unchanged, ensuring deterministic results across clients.

**Q: Can I have multiple start/end pairs?**
A: Currently, the constraint supports one start/end pair. For multiple pairs, create multiple constraint instances.

**Q: What if I want one-way passages?**
A: The current implementation treats all passable edges as bidirectional. For one-way passages, you'd need to customize the constraint.

**Q: Can I validate paths during generation preview?**
A: Yes! The constraint runs during generation, so you can see the path validation in real-time in the editor.

**Q: Does this work with 2D and 3D grids?**
A: Yes! The constraint automatically detects grid type and works with both `UWFCGrid2D` and `UWFCGrid3D`.

**Q: How much does this slow down generation?**
A: With `ValidationFrequency = 10`, typical overhead is 5-15%. With `ValidationFrequency = 50`, overhead is negligible (<2%).

## Future Enhancements

Possible improvements for future versions:
- Multiple start/end pairs for complex dungeons
- Path quality metrics (prefer shorter/longer paths)
- One-way passage support
- Custom heuristics for different path types
- Visual debugging in editor viewport
- Path caching for performance optimization
