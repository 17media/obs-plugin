# package for windows installer

## gather data files into Obs17livePluginData.wxs

```
heat dir {project_base_path}\obs-17live\data -cg Obs17livePluginData -dr DATAFOLDER -gg -g1 -sfrag -srd -out {project_base_path}\package\windows\Obs17Live_Windows_x64_Installer\Obs17livePluginData.wxs
```

replace `SourceDir` in Obs17livePluginData.wxs with the correct path, which should be `{project_base_path}\obs-17live\data`