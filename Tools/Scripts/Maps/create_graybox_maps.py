"""Generate the graybox match maps (C6, first pass):

  /Game/Maps/Graybox/TDM_TwinBases   two mirrored bases across a trench (TDM, 4v4)
  /Game/Maps/Graybox/DM_Pinwheel     one square arena around a raised center (DM)

Run headless (the editor quits when done), with the editor closed:
  powershell -ExecutionPolicy Bypass -File Tools\\Scripts\\Maps\\CreateGrayboxMaps.ps1

Re-running rebuilds each map in place: it removes the actors it made (labels
starting with "GB.") and builds them again, and keeps anything else. These maps
are a first pass to edit by hand: once you edit a "GB." piece, rename it (or
stop re-running the script), or the next run replaces it.

Units are cm; Z=0 is the main floor. Characters step up 45 cm and walk slopes
up to about 44 degrees, so ramps here stay at or under 35 degrees.
"""
import math

import unreal

GAME_MODES = "/Game/Enclave/Game/GameModes"
CUBE = "/Engine/BasicShapes/Cube.Cube"  # 100 cm cube, simple collision, blocks all channels
STAND_Z = 100.0  # capsule center above the floor it stands on
PREFIX = "GB."

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
cube_mesh = unreal.load_asset(CUBE)
team_start_class = unreal.load_class(None, "/Script/Enclave.EnclaveTeamPlayerStart")


# ---------------------------------------------------------------------------
# Building blocks

def spawn(cls, location, rotation=None, label=None):
    rotation = rotation or unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), rotation)
    if label:
        actor.set_actor_label(PREFIX + label)
    return actor


def block(label, lo, hi, rotation=None, center=None):
    """Static box between corners lo and hi (x, y, z), axis-aligned unless rotated about its center."""
    size = [hi[i] - lo[i] for i in range(3)]
    assert all(s > 0 for s in size), (label, lo, hi)
    if center is None:
        center = [(lo[i] + hi[i]) / 2.0 for i in range(3)]
    actor = spawn(unreal.StaticMeshActor, center, rotation, label)
    component = actor.static_mesh_component
    component.set_static_mesh(cube_mesh)
    component.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


def ramp(label, low, high, width, thickness=30.0):
    """A walkable slab whose top surface runs from point low to point high (x, y, z)."""
    dx, dy, dz = (high[i] - low[i] for i in range(3))
    run = math.hypot(dx, dy)
    length = math.hypot(run, dz)
    pitch = math.degrees(math.atan2(dz, run))
    assert pitch <= 35.0, (label, pitch)
    yaw = math.degrees(math.atan2(dy, dx))
    # Drop the center by half the thickness, along the slab's normal, so the top surface meets both points.
    drop = thickness / 2.0 / math.cos(math.radians(pitch))
    center = ((low[0] + high[0]) / 2.0, (low[1] + high[1]) / 2.0, (low[2] + high[2]) / 2.0 - drop)
    half = (length / 2.0, width / 2.0, thickness / 2.0)
    return block(label, (-half[0], -half[1], -half[2]), half,
                 unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw), center)


def yaw_toward(x, y, tx=0.0, ty=0.0):
    return math.degrees(math.atan2(ty - y, tx - x))


def player_start(label, x, y, floor_z, yaw):
    return spawn(unreal.PlayerStart, (x, y, floor_z + STAND_Z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), label)


def team_start(label, team, x, y, floor_z, yaw):
    start = spawn(team_start_class, (x, y, floor_z + STAND_Z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), label)
    start.set_editor_property("team_id", team)
    return start


def lighting():
    spawn(unreal.DirectionalLight, (0.0, 0.0, 2000.0),
          unreal.Rotator(roll=0.0, pitch=-50.0, yaw=30.0), "Sun")
    sky = spawn(unreal.SkyLight, (0.0, 0.0, 1800.0), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, (0.0, 0.0, 0.0), label="SkyAtmosphere")


def build_map(map_path, game_mode, kill_z, build):
    """Opens or creates map_path, replaces its generated actors with build(), sets the mode and Kill Z, and saves.

    Kill Z: actors below it fall out of the world (AActor::CheckStillInWorld), which for a
    player is a death. The engine default is about -10.5 km, minutes of falling.
    """
    if assets.does_asset_exist(map_path):
        if not levels.load_level(map_path):
            raise RuntimeError(f"Could not open {map_path}")
        generated = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith(PREFIX)]
        actors.destroy_actors(generated)
    elif not levels.new_level(map_path):
        raise RuntimeError(f"Could not create {map_path}")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    settings.set_editor_property("default_game_mode", unreal.load_class(None, f"{GAME_MODES}/{game_mode}.{game_mode}_C"))
    settings.set_editor_property("kill_z", kill_z)

    lighting()
    build()
    if not levels.save_current_level():
        raise RuntimeError(f"Could not save {map_path}")
    unreal.log(f"Graybox map saved: {map_path}")


# ---------------------------------------------------------------------------
# TDM_TwinBases
#
# 80 x 40 m, mirrored across X=0. Team 0's base is at -X, team 1's at +X.
#
#   base | courtyard | trench + bridge | courtyard | base
#   -3840   -2600       -600     600      2600      3840
#
# Routes between the bases: the covered bridge over the middle (Z=0), the
# trench floor beneath it (Z=-400), and two flank ramps out of the trench near
# the side walls. Each base has a front door, two side doors and an upper
# balcony (the mezzanine, Z=400) that overlooks the courtyard; the spawn room
# at the back sits behind a sight-blocking wall, with ramps up to the balcony.

TDM_HALF_WIDTH = 2000.0
TRENCH_HALF = 600.0
TRENCH_FLOOR = -400.0
BASE_FRONT = 2600.0
BASE_BACK = 3800.0
WALL = 40.0
BASE_HALF_WIDTH = 1400.0
MEZZANINE = 400.0
ROOF = 800.0


def mirrored(s, a, b):
    """The range [a, b] (written for the + side) on side s (+1 or -1)."""
    return min(s * a, s * b), max(s * a, s * b)


def tdm_side(sx, team):
    """Everything on one side of X=0: sx is +1 or -1; positions are written for +X and mirrored."""
    side = "Blue" if team == 0 else "Red"  # labels only; team colors are C5

    def box(label, x, y, z):
        x0, x1 = mirrored(sx, *x)
        block(f"{side}.{label}", (x0, y[0], z[0]), (x1, y[1], z[1]))

    # Courtyard floor (solid down to the trench floor, so the trench has walls).
    box("Courtyard.Floor", (TRENCH_HALF, BASE_BACK + WALL), (-TDM_HALF_WIDTH, TDM_HALF_WIDTH), (-500.0, 0.0))

    # Courtyard cover.
    for sy in (-1, 1):
        box(f"Courtyard.LowCover.{sy:+d}", (1100.0, 1300.0), mirrored(sy, 700.0, 1100.0), (0.0, 120.0))
        box(f"Courtyard.Crate.{sy:+d}", (1500.0, 1800.0), mirrored(sy, 1350.0, 1650.0), (0.0, 200.0))
        box(f"Courtyard.Pillar.{sy:+d}", (2150.0, 2350.0), mirrored(sy, 1650.0, 1850.0), (0.0, 400.0))
    box("Courtyard.FrontWall", (1900.0, 2100.0), (-250.0, 250.0), (0.0, 250.0))

    # Base: front wall with three doors on the ground floor.
    front = (BASE_FRONT, BASE_FRONT + WALL)
    for n, (y0, y1) in enumerate(((-BASE_HALF_WIDTH, -1200.0), (-900.0, -200.0), (200.0, 900.0), (1200.0, BASE_HALF_WIDTH))):
        box(f"Base.Front.{n + 1}", front, (y0, y1), (0.0, MEZZANINE - 30.0))
    box("Base.Front.Lintel", front, (-BASE_HALF_WIDTH, BASE_HALF_WIDTH), (MEZZANINE - 30.0, MEZZANINE))
    # Balcony: a chest-high parapet with pillars between wide windows.
    box("Base.Parapet", front, (-BASE_HALF_WIDTH, BASE_HALF_WIDTH), (MEZZANINE, MEZZANINE + 110.0))
    for y in (-700.0, 0.0, 700.0):
        box(f"Base.Pillar.{int(y):+d}", front, (y - 50.0, y + 50.0), (MEZZANINE + 110.0, ROOF))

    # Side walls, each with a door under the balcony.
    for sy in (-1, 1):
        wy = mirrored(sy, BASE_HALF_WIDTH, BASE_HALF_WIDTH + WALL)
        box(f"Base.Side.{sy:+d}.Front", (BASE_FRONT, 2900.0), wy, (0.0, ROOF))
        box(f"Base.Side.{sy:+d}.Lintel", (2900.0, 3200.0), wy, (300.0, ROOF))
        box(f"Base.Side.{sy:+d}.Back", (3200.0, BASE_BACK), wy, (0.0, ROOF))

    # Back wall across the whole map, and the roof.
    box("Base.Back", (BASE_BACK, BASE_BACK + WALL), (-TDM_HALF_WIDTH, TDM_HALF_WIDTH), (0.0, ROOF))
    box("Base.Roof", (BASE_FRONT, BASE_BACK + WALL), (-BASE_HALF_WIDTH - WALL, BASE_HALF_WIDTH + WALL), (ROOF, ROOF + 30.0))

    # Mezzanine (the balcony floor) over the front half, ramps up to it from the spawn room.
    box("Base.Mezzanine", (BASE_FRONT + WALL, 3200.0), (-BASE_HALF_WIDTH, BASE_HALF_WIDTH), (MEZZANINE - 30.0, MEZZANINE))
    for sy in (-1, 1):
        ramp(f"{side}.Base.Ramp.{sy:+d}", (sx * (BASE_BACK - 20.0), sy * 1100.0, 0.0),
             (sx * 3200.0, sy * 1100.0, MEZZANINE), 400.0)

    # Spawn room: a wall under the balcony's edge hides it from the front door.
    box("Base.SpawnScreen", (3150.0, 3190.0), (-700.0, 700.0), (0.0, MEZZANINE - 30.0))
    facing = 180.0 if sx > 0 else 0.0
    n = 0
    for x in (3400.0, 3650.0):
        for y in (-600.0, -200.0, 200.0, 600.0):
            n += 1
            team_start(f"{side}.Start.{n}", team, sx * x, y, 0.0, facing)

    # Flank ramp out of the trench on each side, rising toward this side.
    for sy in (-1, 1):
        ramp(f"{side}.Trench.Ramp.{sy:+d}", (0.0, sy * 1600.0, TRENCH_FLOOR), (sx * TRENCH_HALF, sy * 1600.0, 0.0), 600.0)


def build_tdm():
    # Trench floor, cover in it, and the covered bridge over it.
    block("Trench.Floor", (-TRENCH_HALF, -TDM_HALF_WIDTH, -500.0), (TRENCH_HALF, TDM_HALF_WIDTH, TRENCH_FLOOR))
    for sy in (-1, 1):
        y0, y1 = mirrored(sy, 600.0, 1000.0)
        block(f"Trench.Cover.{sy:+d}", (-100.0, y0, TRENCH_FLOOR), (100.0, y1, TRENCH_FLOOR + 150.0))
    block("Bridge.Deck", (-TRENCH_HALF, -300.0, -50.0), (TRENCH_HALF, 300.0, 0.0))
    for sy in (-1, 1):
        y0, y1 = mirrored(sy, 280.0, 300.0)
        block(f"Bridge.Rail.{sy:+d}", (-TRENCH_HALF, y0, 0.0), (TRENCH_HALF, y1, 100.0))
        for sx in (-1, 1):
            block(f"Bridge.Post.{sx:+d}{sy:+d}", (sx * 560.0 - 20.0, sy * 260.0 - 20.0, 0.0), (sx * 560.0 + 20.0, sy * 260.0 + 20.0, 350.0))
    block("Bridge.Roof", (-TRENCH_HALF, -300.0, 350.0), (TRENCH_HALF, 300.0, 380.0))

    # Outer walls along the long sides (the bases close the ends).
    for sy in (-1, 1):
        y0, y1 = mirrored(sy, TDM_HALF_WIDTH, TDM_HALF_WIDTH + WALL)
        block(f"Bound.{sy:+d}", (-BASE_BACK - WALL, y0, -500.0), (BASE_BACK + WALL, y1, 1000.0))

    tdm_side(-1, 0)
    tdm_side(1, 1)


# ---------------------------------------------------------------------------
# DM_Pinwheel
#
# 50 x 50 m square. A raised center (Z=300) with four ramps set like a
# pinwheel (each off-center the same way round), a raised platform in each corner with a ramp along the wall, and a
# screen wall in front of each side's middle that makes a loop behind it.
# Twelve starts spread around: behind the screens, on the corner platforms,
# and on the ground between.

DM_HALF = 2500.0
DM_RAISED = 300.0


def build_dm():
    block("Floor", (-DM_HALF - WALL, -DM_HALF - WALL, -100.0), (DM_HALF + WALL, DM_HALF + WALL, 0.0))
    for n, (lo, hi) in enumerate((
            ((-DM_HALF - WALL, DM_HALF), (DM_HALF + WALL, DM_HALF + WALL)),
            ((-DM_HALF - WALL, -DM_HALF - WALL), (DM_HALF + WALL, -DM_HALF)),
            ((DM_HALF, -DM_HALF), (DM_HALF + WALL, DM_HALF)),
            ((-DM_HALF - WALL, -DM_HALF), (-DM_HALF, DM_HALF)))):
        block(f"Bound.{n + 1}", (lo[0], lo[1], 0.0), (hi[0], hi[1], 800.0))

    # Raised center and its pinwheel ramps: one straight up each side, each
    # set off-center counter-clockwise.
    block("Center", (-500.0, -500.0, 0.0), (500.0, 500.0, DM_RAISED))
    for n in range(4):
        a = math.radians(90.0 * n)
        nx, ny = round(math.cos(a)), round(math.sin(a))  # outward normal of this side
        ox, oy = -ny * 250.0, nx * 250.0  # counter-clockwise offset along the side
        ramp(f"Center.Ramp.{n + 1}", (nx * 1500.0 + ox, ny * 1500.0 + oy, 0.0), (nx * 500.0 + ox, ny * 500.0 + oy, DM_RAISED), 300.0)

    for sx in (-1, 1):
        for sy in (-1, 1):
            corner = f"{sx:+d}{sy:+d}"
            # Corner platform, with a ramp along the X wall.
            block(f"Corner.{corner}", (min(sx * 1800.0, sx * DM_HALF), min(sy * 1800.0, sy * DM_HALF), 0.0),
                  (max(sx * 1800.0, sx * DM_HALF), max(sy * 1800.0, sy * DM_HALF), DM_RAISED))
            ramp(f"Corner.{corner}.Ramp", (sx * 2350.0, sy * 800.0, 0.0), (sx * 2350.0, sy * 1800.0, DM_RAISED), 300.0)
            # Crouch cover and a pillar between the center and each corner.
            block(f"Cover.{corner}", (sx * 1100.0 - 150.0, sy * 1100.0 - 150.0, 0.0), (sx * 1100.0 + 150.0, sy * 1100.0 + 150.0, 120.0))
            block(f"Pillar.{corner}", (sx * 1500.0 - 100.0, sy * 600.0 - 100.0, 0.0), (sx * 1500.0 + 100.0, sy * 600.0 + 100.0, 400.0))

            player_start(f"Start.Corner.{corner}", sx * 2150.0, sy * 2150.0, DM_RAISED, yaw_toward(sx * 2150.0, sy * 2150.0))
            player_start(f"Start.Ground.{corner}", sx * 700.0, sy * 1600.0, 0.0, yaw_toward(sx * 700.0, sy * 1600.0))

    # Screen walls near the middle of each side, and a start behind each.
    for n, (x, y) in enumerate(((0.0, 1900.0), (0.0, -1900.0), (1900.0, 0.0), (-1900.0, 0.0))):
        along_x = x == 0.0
        half = (500.0, 20.0) if along_x else (20.0, 500.0)
        block(f"Screen.{n + 1}", (x - half[0], y - half[1], 0.0), (x + half[0], y + half[1], 300.0))
        sx_, sy_ = (x * 2200.0 / 1900.0, y * 2200.0 / 1900.0)
        player_start(f"Start.Screen.{n + 1}", sx_, sy_, 0.0, yaw_toward(sx_, sy_))


# ---------------------------------------------------------------------------

# Kill Z a few meters below the lowest floor: the trench (-400) in TDM, the arena floor (0) in DM.
build_map("/Game/Maps/Graybox/TDM_TwinBases", "BP_TDMGameMode", -1500.0, build_tdm)
build_map("/Game/Maps/Graybox/DM_Pinwheel", "BP_DMGameMode", -1000.0, build_dm)
unreal.SystemLibrary.quit_editor()
