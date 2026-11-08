"""Action selection and playback for imported LithTech armatures."""

import math

import bpy
from bpy.props import IntProperty, StringProperty
from bpy_extras import anim_utils
from mathutils import Matrix


def armature(context):
    from .export_abc import find_armature
    return find_armature(context)


def actions_for(arm):
    return sorted((a for a in bpy.data.actions
                   if a.get("lt_rig") == arm.name or
                   ("lt_rig" not in a and a.get("lt_model") == arm.name)),
                  key=lambda a: (a.get("lt_child", ""), a.get("lt_index", 1 << 30), a.name))


def assign(arm, action):
    """Bind the action and its object slot together, including after switching."""
    ad = arm.animation_data or arm.animation_data_create()
    slot = next((s for s in action.slots if s.target_id_type == 'OBJECT'
                 and s.name_display == arm.name), None)
    if slot is None:
        slot = anim_utils.action_get_first_suitable_slot(action, 'OBJECT')
    if slot is None:
        slot = action.slots.new(id_type='OBJECT', name=arm.name)
    ad.action = action
    ad.action_slot = slot
    assign_shape_keys(arm, action)


def vertex_animated_keys(arm):
    """The shape keys (Key datablocks) of the meshes this armature deforms
    whose vertex animation the importer keyed (LithTech 1's .abc v6; see
    import_abc._add_v6_vertex_animation)."""
    out = []
    for o in bpy.data.objects:
        if o.type != 'MESH' or o.get("lt_vertex_animation") != "abc6" or o.data.shape_keys is None:
            continue
        if o.parent == arm or any(m.type == 'ARMATURE' and m.object == arm for m in o.modifiers):
            out.append(o.data.shape_keys)
    return out


def assign_shape_keys(arm, action):
    """Give each vertex-animated mesh's shape keys the action's KEY slot, so
    the frames play with the bones; with none (or no action), take the
    action off them and set every frame's key to 0, the first pose."""
    for key in vertex_animated_keys(arm):
        slots = [] if action is None else [s for s in action.slots if s.target_id_type == 'KEY']
        slot = next((s for s in slots if s.name_display == key.name), None)
        if slot is None and len(slots) == 1:
            slot = slots[0]
        if slot is None:
            if key.animation_data:
                key.animation_data.action = None
            for kb in key.key_blocks:
                if "|" in kb.name:
                    kb.value = 0.0
            continue
        ad = key.animation_data or key.animation_data_create()
        ad.action = action
        ad.action_slot = slot


def rest_pose(arm):
    if arm.animation_data:
        arm.animation_data.action = None
        for track in arm.animation_data.nla_tracks:
            track.mute = True
    for bone in arm.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    assign_shape_keys(arm, None)


def select_action(context, arm, action):
    if context.screen and context.screen.is_animation_playing:
        bpy.ops.screen.animation_cancel(restore_frame=False)
    rest_pose(arm)
    if action is not None:
        assign(arm, action)
        start, end = action.frame_range
        context.scene.frame_start = math.floor(start)
        context.scene.frame_end = max(math.ceil(end), math.floor(start) + 1)
        context.scene.use_preview_range = False
        context.scene.frame_set(context.scene.frame_start)
    context.view_layer.update()


def _select_index(arm, context):
    index = arm.lt_animation_index
    if 0 <= index < len(bpy.data.actions):
        action = bpy.data.actions[index]
        if action in actions_for(arm):
            select_action(context, arm, action)


def activate(context, arm, action):
    if hasattr(arm, "lt_animation_index"):
        arm.lt_animation_index = list(bpy.data.actions).index(action)
    else:
        select_action(context, arm, action)


class LITHTECH_OT_animation(bpy.types.Operator):
    """Select an imported action and set its playback range; empty means rest pose"""
    bl_idname = "lithtech.animation"
    bl_label = "Select LithTech Animation"
    bl_options = {'REGISTER', 'UNDO'}

    action_name: StringProperty()

    def execute(self, context):
        arm = armature(context)
        if arm is None:
            return {'CANCELLED'}
        action = None
        if self.action_name:
            action = bpy.data.actions.get(self.action_name)
            if action is None or action not in actions_for(arm):
                self.report({'ERROR'}, "Animation no longer exists on this armature")
                return {'CANCELLED'}
        select_action(context, arm, action)
        if action is None:
            arm.lt_animation_index = -1
        return {'FINISHED'}


class LITHTECH_OT_register_animation(bpy.types.Operator):
    """Include the active action in this model's animation list and exports"""
    bl_idname = "lithtech.register_animation"
    bl_label = "Register Active Action"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        arm = armature(context)
        action = arm.animation_data.action if arm and arm.animation_data else None
        if action is None:
            self.report({'ERROR'}, "Select an armature with an active action")
            return {'CANCELLED'}
        if "lt_child" in action:
            self.report({'ERROR'}, "Child-model actions must be edited and exported with their own model")
            return {'CANCELLED'}
        action["lt_model"] = arm.name
        action["lt_rig"] = arm.name
        action.use_fake_user = True
        assign(arm, action)
        return {'FINISHED'}


class LITHTECH_UL_animations(bpy.types.UIList):
    def filter_items(self, context, data, propname):
        arm = armature(context)
        allowed = set(actions_for(arm)) if arm else set()
        return [self.bitflag_filter_item if a in allowed else 0 for a in getattr(data, propname)], []

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index=0, flt_flag=0):
        label = item.name
        if "lt_child" in item:
            label += " (" + item["lt_child"] + ")"
        layout.label(text=label, icon='ACTION')


class LITHTECH_PT_animations(bpy.types.Panel):
    bl_label = "LithTech Animations"
    bl_idname = "LITHTECH_PT_animations"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        arm = armature(context)
        return arm is not None and "lt_scale" in arm

    def draw(self, context):
        arm = armature(context)
        layout = self.layout
        active = arm.animation_data.action if arm.animation_data else None
        layout.label(text=active.name if active else "Rest pose", icon='ACTION' if active else 'ARMATURE_DATA')
        layout.operator(LITHTECH_OT_animation.bl_idname, text="Rest Pose", icon='ARMATURE_DATA').action_name = ""
        row = layout.row(align=True)
        row.operator("screen.animation_play", text="Play / Pause", icon='PLAY')
        row.operator("screen.animation_cancel", text="Stop", icon='PAUSE')
        actions = actions_for(arm)
        if not actions:
            layout.label(text="No imported animations", icon='INFO')
        else:
            layout.template_list("LITHTECH_UL_animations", "", bpy.data, "actions",
                                 arm, "lt_animation_index", rows=8)
        layout.operator(LITHTECH_OT_register_animation.bl_idname, icon='ADD')


classes = (LITHTECH_OT_animation, LITHTECH_OT_register_animation, LITHTECH_UL_animations, LITHTECH_PT_animations)


def register():
    bpy.types.Object.lt_animation_index = IntProperty(default=-1, min=-1, update=_select_index)


def unregister():
    del bpy.types.Object.lt_animation_index
