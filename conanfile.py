from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
from conan.tools.scm import Git
from conan.tools.files import update_conandata, copy
import subprocess
import os

class packageRecipe(ConanFile):

    # Get the directory of the currently executing script
    script_directory = os.path.dirname(os.path.abspath(__file__))
    print("Script Directory:", script_directory)

    name = "CORTEX_COREDUMP"
    version = "1.0.0.0"
    description = "Cortex M CoreDump"

    # Optional metadata
    license = "Copyright @ Power Electronics. All rights reserved"
    topics = ("Power Electronics")

    # Binary compatibility
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps"
    # +V+ ENABLE UNDER DEMAND
    # options = {"shared": [True, False], "fPIC": [True, False]}
    # default_options = {"shared": True, "fPIC": True}

    def requirements(self):
        print("Downloading dependencies if needed...") # to avoid empty method
        

    def layout(self):
        cmake_layout(self)
        # cmake_layout(self, None, '.', 'conan/build')

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package_info(self):
        self.cpp_info.libs = [self.name]
        self.runenv_info.append_path("LD_LIBRARY_PATH", os.path.join(self.package_folder, "lib"))

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def export(self):
        git = Git(self, self.recipe_folder)
        try:
            scm_url, scm_commit = git.get_url_and_commit()
            self.output.info(f"Obtained URL: {scm_url} and {scm_commit}")
            # we store the current url and commit in conandata.yml
            update_conandata(self, {"sources": {"commit": scm_commit, "url": scm_url}})
        except:
            # Repo is dirty, so we save the change as a local change
            update_conandata(self, {"sources": {"commit": "local_change", "url": self.recipe_folder}})

    def source(self):
        # we recover the saved url and commit from conandata.yml and use them to get sources
        git = Git(self)
        sources = self.conan_data["sources"]
        if "local_change" not in sources["commit"] and "git@" in sources["url"]:
            self.output.info(f"Cloning sources from: {sources}")
            git.clone(url=sources["url"], target=".")
            git.checkout(commit=sources["commit"])
            git.run("submodule update --init --recursive")
        else:
            copy(self,pattern="*",src=sources["url"],dst=".")
