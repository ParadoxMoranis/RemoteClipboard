$QtPath = "C:\Qt\6.8.1\mingw_64"
$BuildPath = ".\build"

# Create necessary directories
New-Item -ItemType Directory -Force -Path "$BuildPath\platforms"
New-Item -ItemType Directory -Force -Path "$BuildPath\styles"

# Copy Qt DLLs
Copy-Item "$QtPath\bin\Qt6Core.dll" -Destination $BuildPath
Copy-Item "$QtPath\bin\Qt6Gui.dll" -Destination $BuildPath
Copy-Item "$QtPath\bin\Qt6Widgets.dll" -Destination $BuildPath
Copy-Item "$QtPath\bin\Qt6Network.dll" -Destination $BuildPath

# Copy platform plugin
Copy-Item "$QtPath\plugins\platforms\qwindows.dll" -Destination "$BuildPath\platforms"

# Copy style plugin
Copy-Item "$QtPath\plugins\styles\qwindowsvistastyle.dll" -Destination "$BuildPath\styles"

# Copy additional DLLs that might be needed
Copy-Item "$QtPath\bin\libgcc_s_seh-1.dll" -Destination $BuildPath
Copy-Item "$QtPath\bin\libstdc++-6.dll" -Destination $BuildPath
Copy-Item "$QtPath\bin\libwinpthread-1.dll" -Destination $BuildPath
