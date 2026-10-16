"""Shared helpers for loading the Steam install of Megabonk with UnityPy."""
import os
from pathlib import Path

import UnityPy
from UnityPy.helpers.TypeTreeGenerator import TypeTreeGenerator

GAME_DIR = Path(os.environ.get(
    "MEGABONK_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Megabonk"))
DATA_DIR = GAME_DIR / "Megabonk_Data"
UNITY_VERSION = "2023.2.22f1"


class _Il2CppGenerator(TypeTreeGenerator):
    # UnityPy appends ".dll" to assembly names, but IL2CPP-loaded assemblies
    # are registered without the extension.
    def get_nodes(self, assembly, fullname):
        return super().get_nodes(assembly.removesuffix(".dll"), fullname)


def load_env(with_typetrees: bool = False):
    env = UnityPy.load(str(DATA_DIR))
    if with_typetrees:
        attach_typetrees(env)
    return env


def attach_typetrees(env):
    """Enable reading IL2CPP MonoBehaviour fields.

    Do this *after* any peek_name() filtering: once attached, every
    MonoBehaviour access goes through the generator, which fails on some types.
    """
    gen = _Il2CppGenerator(UNITY_VERSION)
    gen.load_il2cpp((GAME_DIR / "GameAssembly.dll").read_bytes(),
                    (DATA_DIR / "il2cpp_data" / "Metadata" / "global-metadata.dat").read_bytes())
    env.typetree_generator = gen
