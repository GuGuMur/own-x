# Easy Handwritten Digit Recognition

*Credit: <https://gitee.com/kongfanhe/pytorch-tutorial>*

Platform: ZorinOS17

``` shell
nvidia-smi # 准备安装CUDA 12.8
ubuntu-drivers devices
sudo ubuntu-drivers autoinstall

uv add torch torchvision --default-index https://pypi.org/simple --extra-index-url https://download.pytorch.org/whl/cu130
uv sync

du -h --max-depth=0  # 7.9GB 乐
source .venv/bin/activate
uv run main.py
du -h --max-depth=0 # 8.0GB
rm -rf .venv
```
