# package for windows installer

## gather data files into Obs17livePluginData.wxs

```
heat dir {basePath}\obs-17live\data -cg Obs17livePluginData -dr DATAFOLDER -gg -g1 -sfrag -srd -out {installer_project_path\}Obs17livePluginData.wxs
```

update `SourceDir' in Obs17livePluginData.wxs with the correct path.