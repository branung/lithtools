"""LithTech formats for Blender: .abc and .ltb models and .dtx textures."""

import os

import bpy
from bpy.props import (BoolProperty, CollectionProperty, EnumProperty, FloatProperty,
                       IntProperty, StringProperty)
from bpy_extras.io_utils import ExportHelper, ImportHelper, poll_file_object_drop


# --------------------------------------------------------------------- #
#  Models
# --------------------------------------------------------------------- #

class IMPORT_SCENE_OT_lithtech_abc(bpy.types.Operator, ImportHelper):
    """Import LithTech models (.abc): NOLF, AvP2 and AvP2 Primal Hunt, and LithTech 1's v6 (Shogo, Blood 2, KISS)"""
    bl_idname = "import_scene.lithtech_abc"
    bl_label = "Import LithTech Model"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    filename_ext = ".abc"
    filter_glob: StringProperty(default="*.abc", options={'HIDDEN'})
    files: CollectionProperty(type=bpy.types.OperatorFileListElement,
                              options={'HIDDEN', 'SKIP_SAVE'})
    directory: StringProperty(subtype='DIR_PATH', options={'HIDDEN', 'SKIP_SAVE'})

    scale: FloatProperty(
        name="Scale", default=1.0, min=1e-6, soft_min=0.001, soft_max=10.0,
        description="Blender units per LithTech unit (a NOLF character is about 100 "
                    "units tall; 0.018 makes it 1.8 m)")
    face_front: BoolProperty(
        name="Face Front", default=True,
        description="Turn the model to face Blender's front view (-Y); LithTech models "
                    "face +Z, which becomes +Y. Only the armature object is turned, so "
                    "exporting gives the same file")
    import_animations: BoolProperty(name="Animations", default=True,
                                    description="Import the animations as actions")
    child_animations: BoolProperty(
        name="Child Model Animations", default=True,
        description="Also import the animations of the child models the model names "
                    "(such as NOLF's basemodel.abc), when they are next to it")
    import_lods: BoolProperty(name="LODs", default=True,
                              description="Import the lower levels of detail as hidden objects")
    import_sockets: BoolProperty(name="Sockets", default=True,
                                 description="Import the sockets as empties on their bones")
    import_textures: BoolProperty(
        name="Skins", default=True,
        description="Look for the model's skins (SKINS/<model>.dtx beside the MODELS "
                    "directory, then <model>_*.dtx) and use them in the materials")
    texture_dir: StringProperty(
        name="Skin Directory", subtype='DIR_PATH', default="",
        description="Another directory to look for skins in")

    def execute(self, context):
        from . import import_abc
        paths = [os.path.join(self.directory, f.name) for f in self.files if f.name] \
            or [self.filepath]
        for path in paths:
            try:
                import_abc.load(context, path, scale=self.scale, face_front=self.face_front,
                                import_animations=self.import_animations,
                                child_animations=self.child_animations,
                                import_lods=self.import_lods,
                                import_sockets=self.import_sockets,
                                import_textures=self.import_textures,
                                report=self.report,
                                texture_dirs=(bpy.path.abspath(self.texture_dir),)
                                if self.texture_dir else ())
            except Exception as e:  # report every failure, keep going
                self.report({'ERROR'}, "%s: %s" % (os.path.basename(path), e))
                import traceback
                traceback.print_exc()
        return {'FINISHED'}

    def invoke(self, context, event):
        return self.invoke_popup(context) if self.filepath else super().invoke(context, event)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.prop(self, "scale")
        layout.prop(self, "face_front")
        col = layout.column(heading="Include")
        col.prop(self, "import_animations")
        sub = col.column()
        sub.active = self.import_animations
        sub.prop(self, "child_animations")
        col.prop(self, "import_lods")
        col.prop(self, "import_sockets")
        col.prop(self, "import_textures")
        sub = layout.column()
        sub.active = self.import_textures
        sub.prop(self, "texture_dir")


class EXPORT_SCENE_OT_lithtech_abc(bpy.types.Operator, ExportHelper):
    """Export an armature and its meshes as a LithTech model (.abc: version 12, or LithTech 1's version 6)"""
    bl_idname = "export_scene.lithtech_abc"
    bl_label = "Export LithTech Model"
    bl_options = {'PRESET'}

    filename_ext = ".abc"
    filter_glob: StringProperty(default="*.abc", options={'HIDDEN'})

    abc_version: EnumProperty(
        name="Version",
        items=[('AUTO', "As Imported", "Version 6 for a model imported from LithTech 1's .abc v6, "
                "else version 12"),
               ('12', "12", "NOLF, AvP2 and AvP2 Primal Hunt"),
               ('6', "6 (LithTech 1)", "Shogo, Blood 2, KISS Psycho Circus: one mesh, one node per "
                "vertex, and vertex animation from the shape keys \"<action>|<key>\"")],
        default='AUTO')
    scale: FloatProperty(
        name="Scale", default=1.0, min=1e-6, soft_min=0.001, soft_max=1000.0,
        description="LithTech units per Blender unit (set from the scale the model was "
                    "imported at)")
    use_selection: BoolProperty(name="Selected Only", default=False,
                                description="Only the selected meshes and sockets")
    export_animations: BoolProperty(
        name="Animations", default=True,
        description="Write the armature's actions; without them the model gets one "
                    "animation, \"base\", at the rest pose")
    normalize_weights: BoolProperty(
        name="Normalize Weights", default=True,
        description="Make each vertex's weights sum to 1, as the engine expects")
    export_textures: BoolProperty(
        name="Skins", default=False,
        description="Also write the materials' images as .dtx files next to the model")
    texture_format: EnumProperty(
        name="Skin Format",
        items=[('AUTO', "As Imported", "The format the image was imported in, else 32-bit")] +
              [(i[0], i[1], i[2]) for i in __import__(
                  __package__ + ".textures", fromlist=["BPP_ITEMS"]).BPP_ITEMS],
        default='AUTO')

    def invoke(self, context, event):
        from . import export_abc
        arm = export_abc.find_armature(context)
        if arm is None:
            # Before the file browser: picking a file there overwrites it.
            self.report({'ERROR'}, export_abc.nothing_to_export(context))
            return {'CANCELLED'}
        if "lt_scale" in arm and arm["lt_scale"]:
            self.scale = 1.0 / arm["lt_scale"]
        if not self.filepath or os.path.basename(self.filepath) == "untitled.abc":
            self.filepath = bpy.path.ensure_ext(arm.name, self.filename_ext)
        return super().invoke(context, event)

    def _version(self, context):
        if self.abc_version != 'AUTO':
            return int(self.abc_version)
        from . import export_abc
        arm = export_abc.find_armature(context)
        return 6 if arm is not None and arm.get("lt_source_version") == 6 else 12

    def execute(self, context):
        from . import export_abc, textures
        if self._version(context) == 6:
            from . import export_abc6
            try:
                _, written, _notes = export_abc6.save(
                    context, self.filepath, scale=self.scale, use_selection=self.use_selection,
                    export_animations=self.export_animations, report=self.report)
            except export_abc.ExportError as e:
                self.report({'ERROR'}, str(e))
                return {'CANCELLED'}
            self.report({'INFO'}, "Wrote " + ", ".join(os.path.basename(p) for p in written) + " (v6)")
            return {'FINISHED'}
        try:
            _, written = export_abc.save(
                context, self.filepath, scale=self.scale, use_selection=self.use_selection,
                export_animations=self.export_animations,
                normalize_weights=self.normalize_weights,
                export_textures=self.export_textures,
                texture_bpp=None if self.texture_format == 'AUTO'
                else textures.BPP_BY_ID[self.texture_format])
        except export_abc.ExportError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        self.report({'INFO'}, "Wrote " + ", ".join(os.path.basename(p) for p in written))
        return {'FINISHED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.prop(self, "abc_version")
        v12 = self._version(context) != 6
        layout.prop(self, "scale")
        layout.prop(self, "use_selection")
        layout.prop(self, "export_animations")
        # v6 holds one node per vertex, and LithTech 1's skins are a DTX
        # version the texture writer doesn't write.
        sub = layout.column()
        sub.active = v12
        sub.prop(self, "normalize_weights")
        sub.prop(self, "export_textures")
        sub = layout.column()
        sub.active = v12 and self.export_textures
        sub.prop(self, "texture_format")


class IMPORT_SCENE_OT_lithtech_ltb(bpy.types.Operator, ImportHelper):
    """Import LithTech models (.ltb): NOLF2, Contract J.A.C.K., Tron 2.0 and NOLF on the PS2"""
    bl_idname = "import_scene.lithtech_ltb"
    bl_label = "Import LithTech Model"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    filename_ext = ".ltb"
    filter_glob: StringProperty(default="*.ltb", options={'HIDDEN'})
    files: CollectionProperty(type=bpy.types.OperatorFileListElement,
                              options={'HIDDEN', 'SKIP_SAVE'})
    directory: StringProperty(subtype='DIR_PATH', options={'HIDDEN', 'SKIP_SAVE'})

    scale: FloatProperty(
        name="Scale", default=1.0, min=1e-6, soft_min=0.001, soft_max=10.0,
        description="Blender units per LithTech unit (a NOLF character is about 100 "
                    "units tall; 0.018 makes it 1.8 m)")
    face_front: BoolProperty(
        name="Face Front", default=True,
        description="Turn the model to face Blender's front view (-Y); LithTech models "
                    "face +Z, which becomes +Y. Only the armature object is turned, so "
                    "exporting gives the same file")
    import_animations: BoolProperty(name="Animations", default=True,
                                    description="Import the animations as actions")
    child_animations: BoolProperty(
        name="Child Model Animations", default=True,
        description="Also import the animations of the child models the model names "
                    "(such as NOLF2's base models), when they are next to it")
    import_lods: BoolProperty(name="LODs", default=True,
                              description="Import the lower levels of detail as hidden objects")
    import_sockets: BoolProperty(name="Sockets", default=True,
                                 description="Import the sockets as empties on their bones")
    import_textures: BoolProperty(
        name="Skins", default=True,
        description="Look for the model's skins (SKINS/<model>.dtx beside the MODELS "
                    "directory, then <model>_*.dtx) and use them in the materials")
    texture_dir: StringProperty(
        name="Skin Directory", subtype='DIR_PATH', default="",
        description="Another directory to look for skins in")

    def execute(self, context):
        from . import import_abc, import_ltb, import_ps2ltb
        from .lithtech import dhnp, ps2ltb
        paths = [os.path.join(self.directory, f.name) for f in self.files if f.name] \
            or [self.filepath]
        for path in paths:
            try:
                with open(path, "rb") as fh:
                    head = fh.read(20)
                # PS2 models (file type 2) share the extension
                loader = import_abc if dhnp.is_model(head) else (import_ps2ltb if ps2ltb.is_ps2_model(head) else import_ltb)
                loader.load(context, path, scale=self.scale, face_front=self.face_front,
                                import_animations=self.import_animations,
                                child_animations=self.child_animations,
                                import_lods=self.import_lods,
                                import_sockets=self.import_sockets,
                                import_textures=self.import_textures,
                                report=self.report,
                                texture_dirs=(bpy.path.abspath(self.texture_dir),)
                                if self.texture_dir else ())
            except Exception as e:  # report every failure, keep going
                self.report({'ERROR'}, "%s: %s" % (os.path.basename(path), e))
                import traceback
                traceback.print_exc()
        return {'FINISHED'}

    def invoke(self, context, event):
        return self.invoke_popup(context) if self.filepath else super().invoke(context, event)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.prop(self, "scale")
        layout.prop(self, "face_front")
        col = layout.column(heading="Include")
        col.prop(self, "import_animations")
        sub = col.column()
        sub.active = self.import_animations
        sub.prop(self, "child_animations")
        col.prop(self, "import_lods")
        col.prop(self, "import_sockets")
        col.prop(self, "import_textures")
        sub = layout.column()
        sub.active = self.import_textures
        sub.prop(self, "texture_dir")


class EXPORT_SCENE_OT_lithtech_ltb(bpy.types.Operator, ExportHelper):
    """Export an armature and its meshes as a LithTech model (.ltb: NOLF2, Contract J.A.C.K., Tron 2.0)"""
    bl_idname = "export_scene.lithtech_ltb"
    bl_label = "Export LithTech Model (LTB)"
    bl_options = {'PRESET'}

    filename_ext = ".ltb"
    filter_glob: StringProperty(default="*.ltb", options={'HIDDEN'})

    scale: FloatProperty(
        name="Scale", default=1.0, min=1e-6, soft_min=0.001, soft_max=1000.0,
        description="LithTech units per Blender unit (set from the scale the model was "
                    "imported at)")
    use_selection: BoolProperty(name="Selected Only", default=False,
                                description="Only the selected meshes and sockets")
    export_animations: BoolProperty(
        name="Animations", default=True,
        description="Write the armature's actions; without them the model gets one "
                    "animation, \"base\", at the rest pose")
    version: EnumProperty(
        name="Version",
        items=[('AUTO', "As Imported", "The model file version it was imported from, else 23"),
               ('23', "23 (NOLF2, Contract J.A.C.K., Tron 2.0)", "What the shipped engines load"),
               ('24', "24", "Jupiter Build 69 samples"),
               ('25', "25", "Jupiter Build 69 (MODEL_FILE_VERSION)")],
        default='AUTO')

    def invoke(self, context, event):
        from . import export_abc
        arm = export_abc.find_armature(context)
        if arm is None:
            # Before the file browser: picking a file there overwrites it.
            self.report({'ERROR'}, export_abc.nothing_to_export(context))
            return {'CANCELLED'}
        if "lt_scale" in arm and arm["lt_scale"]:
            self.scale = 1.0 / arm["lt_scale"]
        if not self.filepath or os.path.basename(self.filepath) == "untitled.ltb":
            self.filepath = bpy.path.ensure_ext(arm.name, self.filename_ext)
        return super().invoke(context, event)

    def execute(self, context):
        from . import export_abc, export_ltb
        try:
            _, written = export_ltb.save(
                context, self.filepath, scale=self.scale, use_selection=self.use_selection,
                export_animations=self.export_animations,
                version=None if self.version == 'AUTO' else int(self.version))
        except export_abc.ExportError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        self.report({'INFO'}, "Wrote " + ", ".join(os.path.basename(p) for p in written))
        return {'FINISHED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.prop(self, "scale")
        layout.prop(self, "use_selection")
        layout.prop(self, "export_animations")
        layout.prop(self, "version")


class EXPORT_SCENE_OT_lithtech_ps2ltb(bpy.types.Operator, ExportHelper):
    """Export an armature and its meshes as a PS2 NOLF model (.ltb, file type 2)"""
    bl_idname = "export_scene.lithtech_ps2ltb"
    bl_label = "Export LithTech PS2 Model"
    bl_options = {'PRESET'}

    filename_ext = ".ltb"
    filter_glob: StringProperty(default="*.ltb", options={'HIDDEN'})

    scale: FloatProperty(
        name="Scale", default=1.0, min=1e-6, soft_min=0.001, soft_max=1000.0,
        description="LithTech units per Blender unit (set from the scale the model was "
                    "imported at)")
    use_selection: BoolProperty(name="Selected Only", default=False,
                                description="Only the selected meshes and sockets")
    export_animations: BoolProperty(
        name="Animations", default=True,
        description="Write the armature's actions; without them the model gets one "
                    "animation, \"base\", at the rest pose")

    def invoke(self, context, event):
        from . import export_abc
        arm = export_abc.find_armature(context)
        if arm is not None:
            if "lt_scale" in arm and arm["lt_scale"]:
                self.scale = 1.0 / arm["lt_scale"]
            if not self.filepath or os.path.basename(self.filepath) == "untitled.ltb":
                self.filepath = bpy.path.ensure_ext(arm.name, self.filename_ext)
        return super().invoke(context, event)

    def execute(self, context):
        from . import export_abc, export_ps2ltb
        try:
            _, written = export_ps2ltb.save(
                context, self.filepath, scale=self.scale, use_selection=self.use_selection,
                export_animations=self.export_animations)
        except export_abc.ExportError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        self.report({'INFO'}, "Wrote " + ", ".join(os.path.basename(p) for p in written))
        return {'FINISHED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.prop(self, "scale")
        layout.prop(self, "use_selection")
        layout.prop(self, "export_animations")


# --------------------------------------------------------------------- #
#  Textures
# --------------------------------------------------------------------- #

class IMPORT_IMAGE_OT_lithtech_dtx(bpy.types.Operator, ImportHelper):
    """Open LithTech textures (.dtx) as images"""
    bl_idname = "import_image.lithtech_dtx"
    bl_label = "Import LithTech Texture"
    bl_options = {'REGISTER', 'UNDO'}

    filename_ext = ".dtx"
    filter_glob: StringProperty(default="*.dtx", options={'HIDDEN'})
    files: CollectionProperty(type=bpy.types.OperatorFileListElement,
                              options={'HIDDEN', 'SKIP_SAVE'})
    directory: StringProperty(subtype='DIR_PATH', options={'HIDDEN', 'SKIP_SAVE'})
    palette_alpha: EnumProperty(
        name="Palette Alpha",
        description="How a palettized (32P) texture's alpha is read",
        items=[('AUTO', "Auto", "As the PS2 draws it when the palette was made for the PS2 "
                                "(DEdit's Convert to 32P), else as stored"),
               ('PS2', "PS2", "0x80 is opaque, and a palette with no alpha is opaque"),
               ('STORED', "Stored", "As stored, as the PC engine draws it")],
        default='AUTO')

    def execute(self, context):
        from . import textures
        paths = [os.path.join(self.directory, f.name) for f in self.files if f.name] \
            or [self.filepath]
        img = None
        ps2 = {'AUTO': None, 'PS2': True, 'STORED': False}[self.palette_alpha]
        for path in paths:
            try:
                img = textures.load_image(path, ps2_alpha=ps2)
            except (OSError, ValueError) as e:
                self.report({'ERROR'}, "%s: %s" % (os.path.basename(path), e))
        if img is not None:
            space = context.space_data
            if space is not None and space.type == 'IMAGE_EDITOR':
                space.image = img
            self.report({'INFO'}, "Loaded %d texture(s); see the Image Editor" % len(paths))
        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context) if self.filepath else super().invoke(context, event)


def _image_items(self, context):
    return [(img.name, img.name, "") for img in bpy.data.images
            if img.type == 'IMAGE' or img.source in {'FILE', 'GENERATED'}] or \
        [('', "(no images)", "")]


class EXPORT_IMAGE_OT_lithtech_dtx(bpy.types.Operator, ExportHelper):
    """Save an image as a LithTech texture (.dtx)"""
    bl_idname = "export_image.lithtech_dtx"
    bl_label = "Export LithTech Texture"

    filename_ext = ".dtx"
    filter_glob: StringProperty(default="*.dtx", options={'HIDDEN'})

    image: EnumProperty(name="Image", items=_image_items)
    texture_format: EnumProperty(
        name="Format",
        items=[('AUTO', "As Imported", "The format the image was imported in, else 32-bit")] +
              [(i[0], i[1], i[2]) for i in __import__(
                  __package__ + ".textures", fromlist=["BPP_ITEMS"]).BPP_ITEMS],
        default='AUTO')
    mipmaps: IntProperty(name="Mipmaps", default=0, min=0, max=8,
                         description="Levels to write; 0 for as imported, else down to 8 pixels")

    def invoke(self, context, event):
        space = context.space_data
        img = space.image if space is not None and space.type == 'IMAGE_EDITOR' else None
        if img is None and context.object is not None and context.object.active_material:
            from . import textures
            img = textures.material_image(context.object.active_material)
        if img is not None:
            self.image = img.name
            self.filepath = bpy.path.ensure_ext(os.path.splitext(img.name)[0], ".dtx")
        return super().invoke(context, event)

    def execute(self, context):
        from . import textures
        img = bpy.data.images.get(self.image)
        if img is None:
            self.report({'ERROR'}, "no image to export")
            return {'CANCELLED'}
        w, h = img.size
        if w & (w - 1) or h & (h - 1) or not w or not h:
            self.report({'ERROR'}, "LithTech textures must be powers of two (this is %dx%d)" % (w, h))
            return {'CANCELLED'}
        textures.save_image(img, self.filepath,
                            bpp=None if self.texture_format == 'AUTO'
                            else textures.BPP_BY_ID[self.texture_format],
                            mipmaps=self.mipmaps or None)
        self.report({'INFO'}, "Wrote " + os.path.basename(self.filepath))
        return {'FINISHED'}


# --------------------------------------------------------------------- #
#  Drag and drop, menus
# --------------------------------------------------------------------- #

class IO_FH_lithtech_abc(bpy.types.FileHandler):
    bl_idname = "IO_FH_lithtech_abc"
    bl_label = "LithTech Model"
    bl_import_operator = "import_scene.lithtech_abc"
    bl_export_operator = "export_scene.lithtech_abc"
    bl_file_extensions = ".abc"

    @classmethod
    def poll_drop(cls, context):
        return poll_file_object_drop(context)


class IO_FH_lithtech_ltb(bpy.types.FileHandler):
    bl_idname = "IO_FH_lithtech_ltb"
    bl_label = "LithTech Model (LTB)"
    bl_import_operator = "import_scene.lithtech_ltb"
    bl_export_operator = "export_scene.lithtech_ltb"
    bl_file_extensions = ".ltb"

    @classmethod
    def poll_drop(cls, context):
        return poll_file_object_drop(context)


class IO_FH_lithtech_dtx(bpy.types.FileHandler):
    bl_idname = "IO_FH_lithtech_dtx"
    bl_label = "LithTech Texture"
    bl_import_operator = "import_image.lithtech_dtx"
    bl_file_extensions = ".dtx"

    @classmethod
    def poll_drop(cls, context):
        return context.area is not None and context.area.type in {'IMAGE_EDITOR', 'VIEW_3D'}


def menu_import(self, context):
    self.layout.operator(IMPORT_SCENE_OT_lithtech_abc.bl_idname, text="LithTech Model (.abc)")
    self.layout.operator(IMPORT_SCENE_OT_lithtech_ltb.bl_idname, text="LithTech Model (.ltb)")
    self.layout.operator(IMPORT_IMAGE_OT_lithtech_dtx.bl_idname, text="LithTech Texture (.dtx)")


def menu_export(self, context):
    self.layout.operator(EXPORT_SCENE_OT_lithtech_abc.bl_idname, text="LithTech Model (.abc)")
    self.layout.operator(EXPORT_SCENE_OT_lithtech_ltb.bl_idname, text="LithTech Model (.ltb)")
    self.layout.operator(EXPORT_SCENE_OT_lithtech_ps2ltb.bl_idname, text="LithTech PS2 Model (.ltb)")
    self.layout.operator(EXPORT_IMAGE_OT_lithtech_dtx.bl_idname, text="LithTech Texture (.dtx)")


def menu_image(self, context):
    self.layout.separator()
    self.layout.operator(IMPORT_IMAGE_OT_lithtech_dtx.bl_idname, text="Open LithTech Texture (.dtx)")
    self.layout.operator(EXPORT_IMAGE_OT_lithtech_dtx.bl_idname, text="Save as LithTech Texture (.dtx)")


from . import animation


classes = animation.classes + (
    IMPORT_SCENE_OT_lithtech_ltb,
    EXPORT_SCENE_OT_lithtech_ltb,
    EXPORT_SCENE_OT_lithtech_ps2ltb,
    IMPORT_SCENE_OT_lithtech_abc,
    EXPORT_SCENE_OT_lithtech_abc,
    IMPORT_IMAGE_OT_lithtech_dtx,
    EXPORT_IMAGE_OT_lithtech_dtx,
    IO_FH_lithtech_abc,
    IO_FH_lithtech_ltb,
    IO_FH_lithtech_dtx,
)


def register():
    for c in classes:
        bpy.utils.register_class(c)
    animation.register()
    bpy.types.TOPBAR_MT_file_import.append(menu_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_export)
    bpy.types.IMAGE_MT_image.append(menu_image)


def unregister():
    animation.unregister()
    bpy.types.IMAGE_MT_image.remove(menu_image)
    bpy.types.TOPBAR_MT_file_export.remove(menu_export)
    bpy.types.TOPBAR_MT_file_import.remove(menu_import)
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
