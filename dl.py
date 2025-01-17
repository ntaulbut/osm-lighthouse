from turtledemo.chaos import f
import requests, re, glob, os, shutil


def download_file(url):
    local_filename = url.split('/')[-1]
    with requests.get(url, stream=True) as r:
        with open(local_filename, 'wb') as f:
            shutil.copyfileobj(r.raw, f) # type: ignore

    return local_filename


def state_file_url(time_unit: str) -> str:
    return f"https://planet.openstreetmap.org/replication/{time_unit}/state.txt"


def seq_num(state_file: str) -> int:
    return int(re.findall(r"(?<=sequenceNumber=)\d+(?=\n)", state_file)[0])


def cs_seq_num(state_file: str) -> int:
    return int(re.findall(r"(?<=sequence: )\d+(?=\n)", state_file)[0])


def seq_num_parts(seq_num: int) -> list[int]:
    c: int = seq_num % 1000
    intermediate: int = (seq_num - c) % 1000000
    b: int = int(intermediate / 1000)
    a: int = int((seq_num - intermediate - c) / 1000000)
    return [a, b, c]


def cs_seq_num_parts(cs_seq_num: int) -> list[int]:
    _seq_num_parts = seq_num_parts(cs_seq_num)
    _seq_num_parts[2] += 1
    return _seq_num_parts


def need_seq_nums(current: int, latest: int):
    for i in range(latest - current):
        yield current + i + 1


with open("local-state.csv", "r") as file:
    c_day, c_hr, c_min, c_cs = [int(s) for s in file.read().split(",")]

with open("local-state.csv", "w") as file:
    file.write(",".join([str(n) for n in [c_day, c_hr, c_min, c_cs]]))

CURRENT = 6349937
CS_STATE = "https://planet.openstreetmap.org/replication/changesets/state.yaml"


n = cs_seq_num(requests.get(CS_STATE).text)

print(n, seq_num_parts(n))
print(list(need_seq_nums(CURRENT, n)))

# URL = "https://planet.osm.org/replication/day/000/004/"

# r = requests.get(URL)
# b = r.text
# hrefs = re.findall(r'(?<=href=").*?(?=")', b)
# to_dl = [href for href in hrefs if href.endswith((".gz", ".txt"))]
# print(to_dl)
# for f in to_dl:
#     download_file(URL + f)


# for fp in glob.glob("*.txt"):
#     with open(fp, "r") as f:
#         b = fp.split(".")[0]
#         a = re.findall(r"(?<=sequenceNumber=)\d+(?=\n)", f.read())[0]
#         os.rename(f"{b}.osc.gz", f"{a}.osc.gz")
