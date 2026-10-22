## Plugin browser e 0.11.0

The plugin browser uses the FaseGroove groove browser: manufacturers on the left, plugins on the right, search, favourites and recents. FaseGroove engines are the first group. One click loads a VST3/AU instrument or a single insert effect; the project stores both plugins and their state.

Startup scans the operating-system VST3 folders and one custom folder, including subfolders. The known list is JUCE KnownPluginList XML, so unchanged plugins are not inspected again. Failed scans stay quarantined. RESCAN is in the browser and in plugin settings.

Projects saved by earlier releases remain readable.
