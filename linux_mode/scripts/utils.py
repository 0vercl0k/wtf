import json
import os.path

SYMBOL_STORE = 'symbol-store.json'

def write_to_store(content):

	if not os.path.isfile(SYMBOL_STORE):
		with open(SYMBOL_STORE, 'w'): pass
	
	data = {}
		
	with open(SYMBOL_STORE, 'r') as f:
		try:
			data = json.load(f)
		except:
			pass
			
	data.update(content)
		
	with open(SYMBOL_STORE, 'w') as f:	
		json.dump(data, f)
