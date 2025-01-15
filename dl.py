import requests, re, glob, os, shutil


def download_file(url):
    local_filename = url.split('/')[-1]
    with requests.get(url, stream=True) as r:
        with open(local_filename, 'wb') as f:
            shutil.copyfileobj(r.raw, f) # type: ignore

    return local_filename


URL = "https://planet.osm.org/replication/day/000/004/"

r = requests.get(URL)
b = r.text
hrefs = re.findall(r'(?<=href=").*?(?=")', b)
to_dl = [href for href in hrefs if href.endswith((".gz", ".txt"))]
print(to_dl)
for f in to_dl:
    download_file(URL + f)


for fp in glob.glob("*.txt"):
    with open(fp, "r") as f:
        b = fp.split(".")[0]
        a = re.findall(r"(?<=sequenceNumber=)\d+(?=\n)", f.read())[0]
        os.rename(f"{b}.osc.gz", f"{a}.osc.gz")
