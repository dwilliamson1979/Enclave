"""Generate /Game/Maps/Test/SSR_TestRange, the SSR test range.

Run headless (the editor quits when done):
  UnrealEditor.exe Enclave.uproject -ExecutePythonScript="<repo>/Tools/Scripts/SSR/create_ssr_test_map.py" -unattended -nosplash
Or use Tools/Scripts/SSR/CreateSSRTestMap.ps1. Re-running replaces the map.

Layout (cm). Lanes run along +X and sit 20 m apart in Y, so shots cannot cross lanes.
In every lane the shooter stands at X=0 facing +X and the target at X=1500
facing the shooter, so the target's strafe is lateral to the shooter's view.
Markers are TargetPoints tagged:
  SSR.<lane>.Shooter, SSR.<lane>.Target, SSR.<lane>.Bystander.<n>   (n from 1)
  SSR.Holding.<n>   parking for bystanders a lane does not use
The SSR test subsystem (Enclave.Debug.SSRTest.Layout <lane>) teleports
participants to these markers before a session starts.
"""
import unreal

MAP_PATH = "/Game/Maps/Test/SSR_TestRange"
GAME_MODE = "/Game/Enclave/Game/GameModes/BP_EnclaveGameModeBase.BP_EnclaveGameModeBase_C"
CUBE = "/Engine/BasicShapes/Cube.Cube"  # 100 cm cube, simple collision, blocks all channels
STAND_Z = 100.0  # capsule center above the floor
LANE_SPACING = 2000.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
cube_mesh = unreal.load_asset(CUBE)


def spawn(cls, location, yaw=0.0, label=None):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(0.0, 0.0, yaw))
    if label:
        actor.set_actor_label(label)
    return actor


def box(label, center, size):
    """Axis-aligned static box: center (x, y, z) and size (x, y, z) in cm."""
    actor = spawn(unreal.StaticMeshActor, center, label=label)
    component = actor.static_mesh_component
    component.set_static_mesh(cube_mesh)
    component.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


def marker(tag, x, y, yaw):
    point = spawn(unreal.TargetPoint, (x, y, STAND_Z), yaw, label=tag)
    point.tags = [unreal.Name(tag)]
    return point


def lane(name, index, bystanders=(), walls=()):
    """bystanders: (dx, dy) offsets from the lane origin; walls: (center, size) relative to it."""
    y = index * LANE_SPACING
    marker(f"SSR.{name}.Shooter", 0.0, y, 0.0)
    marker(f"SSR.{name}.Target", 1500.0, y, 180.0)
    for n, (dx, dy) in enumerate(bystanders, start=1):
        marker(f"SSR.{name}.Bystander.{n}", dx, y + dy, 180.0)
    for n, (center, size) in enumerate(walls, start=1):
        box(f"SSR.{name}.Wall.{n}", (center[0], y + center[1], center[2]), size)


# Re-running rebuilds in place: open the existing map and remove what this script made
# (matched by label), so the asset and anything referencing it keep their identity.
GENERATED_PREFIXES = ("SSR.", "PlayerStart.", "Floor", "Sun", "SkyLight")
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
if assets.does_asset_exist(MAP_PATH):
    if not levels.load_level(MAP_PATH):
        raise RuntimeError(f"Could not open {MAP_PATH}")
    generated = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith(GENERATED_PREFIXES)]
    actors.destroy_actors(generated)
elif not levels.new_level(MAP_PATH):
    raise RuntimeError(f"Could not create {MAP_PATH}")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
settings = world.get_world_settings()
settings.set_editor_property("default_game_mode", unreal.load_class(None, GAME_MODE))

# Floor: top surface at Z=0. Covers X -70..+50 m (parking markers reach -64 m, lanes +22 m)
# and Y -40..+155 m (holding area from -30 m, last lane at +140 m).
box("Floor", (-1000.0, 5750.0, -50.0), (12000.0, 19500.0, 100.0))

# Lighting: enough for the shooter's view; nothing here affects collision.
spawn(unreal.DirectionalLight, (0.0, 0.0, 1000.0), label="Sun").set_actor_rotation(unreal.Rotator(0.0, -50.0, 30.0), False)
spawn(unreal.SkyLight, (0.0, 0.0, 800.0), label="SkyLight")

# Holding area: 12 PlayerStarts and 12 parking markers, far from every lane.
for n in range(12):
    px = -4000.0 - 300.0 * (n % 4)
    py = -3000.0 + 300.0 * (n // 4)
    spawn(unreal.PlayerStart, (px, py, STAND_Z), 0.0, label=f"PlayerStart.{n + 1}")
    marker(f"SSR.Holding.{n + 1}", px - 1500.0, py, 0.0)

lane("open", 0)
lane("crossing", 1, bystanders=[(750.0, 0.0)])
lane("backstop", 2, bystanders=[(1800.0, 0.0)])
# Full wall (4 m, well above any head line) covering the +Y half of the target's +/-250 cm strafe.
lane("cover", 3, walls=[((1300.0, 200.0, 200.0), (40.0, 400.0, 400.0))])
# Low wall across the whole lane: blocks a center-mass line from a standing shooter.
lane("halfcover", 4, walls=[((1300.0, 0.0, 55.0), (40.0, 1000.0, 110.0))])
# "Stonehenge": five 60 cm pillars with 80 cm gaps across the target's strafe, so the
# target moves in and out of cover several times per sweep (edge-timing test).
lane("pillars", 6, walls=[((1300.0, -280.0 + 140.0 * n, 150.0), (60.0, 60.0, 300.0)) for n in range(5)])
# Six bystanders at mixed depths around the shot line, for 8-client runs.
lane("crowd", 7, bystanders=[(500.0, 150.0), (1000.0, -150.0), (1800.0, 0.0),
                             (2200.0, 200.0), (800.0, -350.0), (1300.0, 350.0)])

levels.save_current_level()
unreal.log(f"SSR test map saved: {MAP_PATH}")
unreal.SystemLibrary.quit_editor()
