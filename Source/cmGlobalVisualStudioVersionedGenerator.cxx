/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmGlobalVisualStudioVersionedGenerator.h"

#include "cmAlgorithms.h"
#include "cmDocumentationEntry.h"
#include "cmLocalVisualStudio10Generator.h"
#include "cmMakefile.h"
#include "cmVSSetupHelper.h"

static unsigned int VSVersionToMajor(
  cmGlobalVisualStudioGenerator::VSVersion v)
{
  switch (v) {
    case cmGlobalVisualStudioGenerator::VS9:
      return 9;
    case cmGlobalVisualStudioGenerator::VS10:
      return 10;
    case cmGlobalVisualStudioGenerator::VS11:
      return 11;
    case cmGlobalVisualStudioGenerator::VS12:
      return 12;
    case cmGlobalVisualStudioGenerator::VS14:
      return 14;
    case cmGlobalVisualStudioGenerator::VS15:
      return 15;
  }
  return 0;
}

static const char vs15generatorName[] = "Visual Studio 15 2017";

// Map generator name without year to name with year.
static const char* cmVS15GenName(const std::string& name, std::string& genName)
{
  if (strncmp(name.c_str(), vs15generatorName,
              sizeof(vs15generatorName) - 6) != 0) {
    return 0;
  }
  const char* p = name.c_str() + sizeof(vs15generatorName) - 6;
  if (cmHasLiteralPrefix(p, " 2017")) {
    p += 5;
  }
  genName = std::string(vs15generatorName) + p;
  return p;
}

class cmGlobalVisualStudioVersionedGenerator::Factory15
  : public cmGlobalGeneratorFactory
{
public:
  cmGlobalGenerator* CreateGlobalGenerator(const std::string& name,
                                           cmake* cm) const override
  {
    std::string genName;
    const char* p = cmVS15GenName(name, genName);
    if (!p) {
      return 0;
    }
    if (!*p) {
      return new cmGlobalVisualStudioVersionedGenerator(
        cmGlobalVisualStudioGenerator::VS15, cm, genName, "");
    }
    if (*p++ != ' ') {
      return 0;
    }
    if (strcmp(p, "Win64") == 0) {
      return new cmGlobalVisualStudioVersionedGenerator(
        cmGlobalVisualStudioGenerator::VS15, cm, genName, "x64");
    }
    if (strcmp(p, "ARM") == 0) {
      return new cmGlobalVisualStudioVersionedGenerator(
        cmGlobalVisualStudioGenerator::VS15, cm, genName, "ARM");
    }
    return 0;
  }

  void GetDocumentation(cmDocumentationEntry& entry) const override
  {
    entry.Name = std::string(vs15generatorName) + " [arch]";
    entry.Brief = "Generates Visual Studio 2017 project files.  "
                  "Optional [arch] can be \"Win64\" or \"ARM\".";
  }

  void GetGenerators(std::vector<std::string>& names) const override
  {
    names.push_back(vs15generatorName);
    names.push_back(vs15generatorName + std::string(" ARM"));
    names.push_back(vs15generatorName + std::string(" Win64"));
  }

  bool SupportsToolset() const override { return true; }
  bool SupportsPlatform() const override { return true; }
};

cmGlobalGeneratorFactory*
cmGlobalVisualStudioVersionedGenerator::NewFactory15()
{
  return new Factory15;
}

cmGlobalVisualStudioVersionedGenerator::cmGlobalVisualStudioVersionedGenerator(
  VSVersion version, cmake* cm, const std::string& name,
  std::string const& platformInGeneratorName)
  : cmGlobalVisualStudio14Generator(cm, name, platformInGeneratorName)
  , vsSetupAPIHelper(VSVersionToMajor(version))
{
  this->ExpressEdition = false;
  this->DefaultPlatformToolset = "v141";
  this->DefaultCLFlagTableName = "v141";
  this->DefaultCSharpFlagTableName = "v141";
  this->DefaultLinkFlagTableName = "v141";
  this->Version = version;
}

bool cmGlobalVisualStudioVersionedGenerator::MatchesGeneratorName(
  const std::string& name) const
{
  std::string genName;
  switch (this->Version) {
    case cmGlobalVisualStudioGenerator::VS9:
    case cmGlobalVisualStudioGenerator::VS10:
    case cmGlobalVisualStudioGenerator::VS11:
    case cmGlobalVisualStudioGenerator::VS12:
    case cmGlobalVisualStudioGenerator::VS14:
      break;
    case cmGlobalVisualStudioGenerator::VS15:
      if (cmVS15GenName(name, genName)) {
        return genName == this->GetName();
      }
      break;
  }
  return false;
}

bool cmGlobalVisualStudioVersionedGenerator::SetGeneratorInstance(
  std::string const& i, cmMakefile* mf)
{
  if (!i.empty()) {
    if (!this->vsSetupAPIHelper.SetVSInstance(i)) {
      std::ostringstream e;
      /* clang-format off */
      e <<
        "Generator\n"
        "  " << this->GetName() << "\n"
        "could not find specified instance of Visual Studio:\n"
        "  " << i;
      /* clang-format on */
      mf->IssueMessage(cmake::FATAL_ERROR, e.str());
      return false;
    }
  }

  std::string vsInstance;
  if (!this->vsSetupAPIHelper.GetVSInstanceInfo(vsInstance)) {
    std::ostringstream e;
    /* clang-format off */
    e <<
      "Generator\n"
      "  " << this->GetName() << "\n"
      "could not find any instance of Visual Studio.\n";
    /* clang-format on */
    mf->IssueMessage(cmake::FATAL_ERROR, e.str());
    return false;
  }

  // Save the selected instance persistently.
  std::string genInstance = mf->GetSafeDefinition("CMAKE_GENERATOR_INSTANCE");
  if (vsInstance != genInstance) {
    this->CMakeInstance->AddCacheEntry(
      "CMAKE_GENERATOR_INSTANCE", vsInstance.c_str(),
      "Generator instance identifier.", cmStateEnums::INTERNAL);
  }

  return true;
}

bool cmGlobalVisualStudioVersionedGenerator::GetVSInstance(
  std::string& dir) const
{
  return vsSetupAPIHelper.GetVSInstanceInfo(dir);
}

bool cmGlobalVisualStudioVersionedGenerator::IsDefaultToolset(
  const std::string& version) const
{
  if (version.empty()) {
    return true;
  }

  std::string vcToolsetVersion;
  if (this->vsSetupAPIHelper.GetVCToolsetVersion(vcToolsetVersion)) {

    cmsys::RegularExpression regex("[0-9][0-9]\\.[0-9]+");
    if (regex.find(version) && regex.find(vcToolsetVersion)) {
      const auto majorMinorEnd = vcToolsetVersion.find('.', 3);
      const auto majorMinor = vcToolsetVersion.substr(0, majorMinorEnd);
      return version == majorMinor;
    }
  }

  return false;
}

std::string cmGlobalVisualStudioVersionedGenerator::GetAuxiliaryToolset() const
{
  const char* version = this->GetPlatformToolsetVersion();
  if (version) {
    std::string instancePath;
    GetVSInstance(instancePath);
    std::stringstream path;
    path << instancePath;
    path << "/VC/Auxiliary/Build/";
    path << version;
    path << "/Microsoft.VCToolsVersion." << version << ".props";

    std::string toolsetPath = path.str();
    cmSystemTools::ConvertToUnixSlashes(toolsetPath);
    return toolsetPath;
  }
  return {};
}

bool cmGlobalVisualStudioVersionedGenerator::InitializeWindows(cmMakefile* mf)
{
  // If the Win 8.1 SDK is installed then we can select a SDK matching
  // the target Windows version.
  if (this->IsWin81SDKInstalled()) {
    return cmGlobalVisualStudio14Generator::InitializeWindows(mf);
  }
  // Otherwise we must choose a Win 10 SDK even if we are not targeting
  // Windows 10.
  return this->SelectWindows10SDK(mf, false);
}

bool cmGlobalVisualStudioVersionedGenerator::SelectWindowsStoreToolset(
  std::string& toolset) const
{
  if (cmHasLiteralPrefix(this->SystemVersion, "10.0")) {
    if (this->IsWindowsStoreToolsetInstalled() &&
        this->IsWindowsDesktopToolsetInstalled()) {
      toolset = "v141"; // VS 15 uses v141 toolset
      return true;
    } else {
      return false;
    }
  }
  return this->cmGlobalVisualStudio14Generator::SelectWindowsStoreToolset(
    toolset);
}

bool cmGlobalVisualStudioVersionedGenerator::IsWindowsDesktopToolsetInstalled()
  const
{
  return vsSetupAPIHelper.IsVSInstalled();
}

bool cmGlobalVisualStudioVersionedGenerator::IsWindowsStoreToolsetInstalled()
  const
{
  return vsSetupAPIHelper.IsWin10SDKInstalled();
}

bool cmGlobalVisualStudioVersionedGenerator::IsWin81SDKInstalled() const
{
  // Does the VS installer tool know about one?
  if (vsSetupAPIHelper.IsWin81SDKInstalled()) {
    return true;
  }

  // Does the registry know about one (e.g. from VS 2015)?
  std::string win81Root;
  if (cmSystemTools::ReadRegistryValue(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\"
        "Windows Kits\\Installed Roots;KitsRoot81",
        win81Root, cmSystemTools::KeyWOW64_32) ||
      cmSystemTools::ReadRegistryValue(
        "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\"
        "Windows Kits\\Installed Roots;KitsRoot81",
        win81Root, cmSystemTools::KeyWOW64_32)) {
    return cmSystemTools::FileExists(win81Root + "/um/windows.h", true);
  }
  return false;
}

std::string cmGlobalVisualStudioVersionedGenerator::GetWindows10SDKMaxVersion()
  const
{
  return std::string();
}

std::string cmGlobalVisualStudioVersionedGenerator::FindMSBuildCommand()
{
  std::string msbuild;

  // Ask Visual Studio Installer tool.
  std::string vs;
  if (vsSetupAPIHelper.GetVSInstanceInfo(vs)) {
    msbuild = vs + "/MSBuild/15.0/Bin/MSBuild.exe";
    if (cmSystemTools::FileExists(msbuild)) {
      return msbuild;
    }
  }

  msbuild = "MSBuild.exe";
  return msbuild;
}

std::string cmGlobalVisualStudioVersionedGenerator::FindDevEnvCommand()
{
  std::string devenv;

  // Ask Visual Studio Installer tool.
  std::string vs;
  if (vsSetupAPIHelper.GetVSInstanceInfo(vs)) {
    devenv = vs + "/Common7/IDE/devenv.com";
    if (cmSystemTools::FileExists(devenv)) {
      return devenv;
    }
  }

  devenv = "devenv.com";
  return devenv;
}
