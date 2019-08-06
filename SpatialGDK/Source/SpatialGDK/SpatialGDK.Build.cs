using System;
using System.ComponentModel;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using UnrealBuildTool;

public class SpatialGDK : ModuleRules
{
    private string ProjectRoot
    {
        get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../../")); }
    }

    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ProjectRoot, "Binaries", "ThirdParty")); }
    }

    private string ImprobableThirdPartyDir
    {
        get { return Path.Combine(ThirdPartyPath, "Improbable"); }
    }

    public SpatialGDK(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bFasterWithoutUnity = true;

        PrivateIncludePaths.Add("SpatialGDK/Private");

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "EngineSettings",
                "Projects",
                "OnlineSubsystemUtils",
                "InputCore",
                "Sockets",
            });

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("UnrealEd");
			PublicDependencyModuleNames.Add("SpatialGDKServices");
		}

        if (Target.bWithPerfCounters)
        {
            PublicDependencyModuleNames.Add("PerfCounters");
        }

        var WorkerLibraryDir = Path.GetFullPath(Path.Combine(ImprobableThirdPartyDir, Target.Platform.ToString()));

        string LibPrefix = "";
        string ImportLibSuffix = "";
        string SharedLibSuffix = "";
        bool bAddDelayLoad = false;

        switch (Target.Platform)
        {
            case UnrealTargetPlatform.Win32:
            case UnrealTargetPlatform.Win64:
                ImportLibSuffix = ".lib";
                SharedLibSuffix = ".dll";
                bAddDelayLoad = true;
                break;
            case UnrealTargetPlatform.Mac:
                LibPrefix = "lib";
                ImportLibSuffix = SharedLibSuffix = ".dylib";
                break;
            case UnrealTargetPlatform.Linux:
                LibPrefix = "lib";
                ImportLibSuffix = SharedLibSuffix = ".so";
                break;
            case UnrealTargetPlatform.PS4:
                LibPrefix = "lib";
                ImportLibSuffix = "_stub.a";
                SharedLibSuffix = ".prx";
                bAddDelayLoad = true;
                break;
            case UnrealTargetPlatform.XboxOne:
                ImportLibSuffix = ".lib";
                SharedLibSuffix = ".dll";
                // We don't set bAddDelayLoad = true here, because we get "unresolved external symbol __delayLoadHelper2".
                // See: https://www.fmod.org/questions/question/deploy-issue-on-xboxone-with-unrealengine-4-14/
                break;
            case UnrealTargetPlatform.IOS:
                LibPrefix = "lib";
                ImportLibSuffix = SharedLibSuffix = "_static_fullylinked.a";
                break;
            case UnrealTargetPlatform.Android:
                {
                    LibPrefix = "lib";
                    ImportLibSuffix = ".a";

                    AddSpatialAndroidDependencies(Target);
                    break;
                }
            default:
                throw new System.Exception(System.String.Format("Unsupported platform {0}", Target.Platform.ToString()));
        }

        if (UnrealTargetPlatform.Android != Target.Platform)
        {
            string WorkerImportLib = System.String.Format("{0}worker{1}", LibPrefix, ImportLibSuffix);
            string WorkerSharedLib = System.String.Format("{0}worker{1}", LibPrefix, SharedLibSuffix);

            PublicAdditionalLibraries.AddRange(new[] { Path.Combine(WorkerLibraryDir, WorkerImportLib) });
            PublicLibraryPaths.Add(WorkerLibraryDir);
            RuntimeDependencies.Add(Path.Combine(WorkerLibraryDir, WorkerSharedLib), StagedFileType.NonUFS);
            if (bAddDelayLoad)
            {
                PublicDelayLoadDLLs.Add(WorkerSharedLib);
            }

            string IncludeDir = Path.Combine(WorkerLibraryDir, "include");
            PublicIncludePaths.Add(IncludeDir);

            System.Console.WriteLine("++++++++++++ IncludeDir: " + IncludeDir + "\r");
        }
    }

    public void AddSpatialAndroidDependencies(ReadOnlyTargetRules Target)
    {
        string WorkerLibraryDir = Path.GetFullPath(Path.Combine(ImprobableThirdPartyDir, Target.Platform.ToString()));

        switch (Target.Platform)
        {
            case UnrealTargetPlatform.Android:
                {
                    //AndroidNDKCompilerVersion = NDKCompilerVersion.LLVM;

                    List<string> LibNames = new List<string>() { "libCoreSdk.a", "libgpr.a", "libgrpc.a",
                        "libgrpc++.a", "libprotobuf.a", "libRakNetLibStatic.a", "libWorkerSdk.a", "libz.a"};

                    foreach (string Name in LibNames)
                    {
                        string LibPath = Path.Combine(WorkerLibraryDir, "lib", Name);
                        PublicAdditionalLibraries.Add(LibPath);

                        System.Console.WriteLine("++++++++++++++++++++++++ NDK lib" + Name + " : " + LibPath + "\r");
                    }

                    string IncludePath = Path.Combine(WorkerLibraryDir, "include");
                    PublicIncludePaths.Add(IncludePath);

                    System.Console.WriteLine("++++++++++++++++++++++++ NDK headers path: " + IncludePath + "\r");
                    break;
                }
        }
    }
}
