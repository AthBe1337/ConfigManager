# ConfigManager

使用TUI对应用的json设置进行管理，支持通过schema进行合法性校验、新建、删除等，支持多配置管理。

## 使用方法

启动时添加参数，内容为你的应用名称

```bash
./ConfigManager your_app_name
```
启动后会自动在你的`home`路径下的`.config`中创建一个名为`your_app_name`的文件夹，里面存放你的配置文件。 如果在启动时没有添加参数，则需要手动输入。

然后程序会检查`~/.config/your_app_name`文件夹中是否存在`schema.json`，你可以手动编辑并复制到这个位置，也可以编辑好后在应用中输入路径，程序会自动复制到目标目录。

### 主界面

进入主界面后，你可以在此对现有的配置进行编辑、删除和激活。当设置一个配置文件为激活文件时，首先会跟据schema校验配置文件是否合法，如果校验通过，会在配置文件夹新建一个符号链接，指向此配置文件。

你的应用可以直接使用此配置文件。实现多配置管理。

![](https://cloud.athbe.cn/f/PVho/F%5BVMI4FNBOFR%5B9ZU%7BM~98G1.png)

### 配置编辑界面

![](https://cloud.athbe.cn/f/w3u6/_JIP5@6UUQ9LW%28T%2958H75MJ.png)

你可以在左侧选择配置项，在右侧编辑后点击更新。支持数组元素的添加和删除。

右侧面板会显示当前配置的详细信息(在schema的`description`字段中定义)。

编辑完成后，点击保存配置，此时修改会写入文件。

## 构建

## linux

```bash
git clone --recurse-submodules https://github.com/AthBe1337/ConfigManager.git
cd ConfigManager
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Windows

```bash
git clone --recurse-submodules https://github.com/AthBe1337/ConfigManager.git
cd ConfigManager
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
ninja -j18
```

***由于需要创建符号链接，所以需要管理员权限。启动时请以管理员身份运行。***