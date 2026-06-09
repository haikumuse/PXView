import json
d = json.load(open(r'c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\test_devs.json'))
devs = json.loads(d['result']['content'][0]['text'])
for x in devs:
    print(json.dumps(x, ensure_ascii=False))
