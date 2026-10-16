"""One-time UE editor authoring. Existing assets are never overwritten.

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
The saved WBP remains the source of layout/style; this is not a runtime UI path.
"""
import unreal

WIDGET = "/GarmentFitLab/UI/WBP_GarmentFitLab"
LEVEL = "/Game/Maps/L_GarmentFitLab"
assets = unreal.EditorAssetLibrary


def create_widget():
    if assets.does_asset_exist(WIDGET):
        return assets.load_asset(WIDGET)
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.GarmentFitInputWidget)
    settings = unreal.get_default_object(unreal.load_class(None, "/Script/UMGEditor.UMGEditorProjectSettings"))
    previous_root = settings.get_editor_property("DefaultRootWidget")
    previous_selector = settings.get_editor_property("bUseWidgetTemplateSelector")
    try:
        settings.set_editor_property("DefaultRootWidget", unreal.CanvasPanel)
        settings.set_editor_property("bUseWidgetTemplateSelector", False)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "WBP_GarmentFitLab", "/GarmentFitLab/UI", unreal.WidgetBlueprint, factory)
    finally:
        # In-memory factory settings only. Never write the project's Config.
        settings.set_editor_property("DefaultRootWidget", previous_root)
        settings.set_editor_property("bUseWidgetTemplateSelector", previous_selector)
    tree = unreal.find_object(bp, "WidgetTree")
    root = next(item for item in unreal.ObjectIterator(unreal.CanvasPanel) if item.get_outer() == tree)

    def widget(cls, name):
        return unreal.new_object(cls, outer=tree, name=name)

    panel = widget(unreal.Border, "InputPanel")
    panel.set_editor_property("brush_color", unreal.LinearColor(0.018, 0.026, 0.04, 0.96))
    panel.set_editor_property("padding", unreal.Margin(24, 22, 24, 22))
    slot = root.add_child_to_canvas(panel)
    slot.set_position(unreal.Vector2D(28, 28))
    slot.set_size(unreal.Vector2D(400, 470))
    column = widget(unreal.VerticalBox, "InputColumn")
    panel.add_child(column)

    def text(name, value, size, color, bottom=16):
        item = widget(unreal.TextBlock, name)
        item.set_text(value)
        font = item.get_editor_property("font")
        font.set_editor_property("size", size)
        item.set_editor_property("font", font)
        item.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(*color)))
        item.set_auto_wrap_text(True)
        child_slot = column.add_child_to_vertical_box(item)
        child_slot.set_padding(unreal.Margin(0, 0, 0, bottom))
        return item

    text("StageLabel", "GARMENT FIT LAB  /  01", 14, (0.35, 0.8, 0.75, 1))
    text("TitleText", "캐릭터 입력", 28, (0.95, 0.97, 1, 1), 24)
    text("InputStatusText", "캐릭터 메시 대기 중\n레벨의 GarmentFitPreview 액터에서 Input Mesh를 지정하세요.",
                  17, (0.95, 0.8, 0.42, 1), 20)
    text("InputAssetText", "입력 애셋: 없음", 13, (0.66, 0.73, 0.82, 1), 24)
    text("InstructionsText", "1. Play를 종료하고 미리보기 액터 선택\n2. Details > Input Mesh에 메시 지정\n3. 크기·방향·포즈를 확인", 15,
         (0.85, 0.88, 0.93, 1), 22)
    text("ScopeText", "단계 1 · 메시 표시 준비\n체형 분석 · 옷본 · 봉제 · 시뮬레이션은 후속 단계입니다.", 13,
         (0.52, 0.61, 0.71, 1), 0)
    if not unreal.BlueprintEditorLibrary.compile_blueprint(bp):
        raise RuntimeError("Widget Blueprint compile failed")
    if not assets.save_loaded_asset(bp):
        raise RuntimeError("Widget Blueprint save failed")
    return bp


def create_level(bp):
    if assets.does_asset_exist(LEVEL):
        return
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(LEVEL):
        raise RuntimeError("Could not create test level")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    preview = actors.spawn_actor_from_class(unreal.GarmentFitPreviewActor, unreal.Vector(0, 0, 0))
    preview.set_actor_label("GarmentFitPreview")
    preview.set_editor_property("input_widget_class", bp.generated_class())
    preview.refresh_input()

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.GarmentFitLabGameMode)
    view_position = unreal.Vector(360, -480, 240)
    view_rotation = unreal.MathLibrary.find_look_at_rotation(view_position, unreal.Vector(0, 0, 90))
    start = actors.spawn_actor_from_class(unreal.PlayerStart, view_position, view_rotation)
    start.set_actor_label("Lab_View_Start")
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -2))
    floor.set_actor_label("Lab_Floor")
    floor.static_mesh_component.set_static_mesh(assets.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(12, 12, 1))
    light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 400), unreal.Rotator(pitch=-45, yaw=-35, roll=0))
    light.set_actor_label("Lab_Key_Light")
    light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.light_component.set_editor_property("intensity", 4.0)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 250))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(view_position, view_rotation)
    actors.set_selected_level_actors([preview])
    if not levels.save_current_level():
        raise RuntimeError("Could not save test level")


create_level(create_widget())
unreal.log("GARMENT_FIT_LAB_AUTHORING_OK")
