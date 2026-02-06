from datetime import datetime

def serialize(doc):
    if not doc:
        return {}

    if "_id" in doc:
        doc["_id"] = str(doc["_id"])
    return doc


def calculate_mood(moisture: float):
    if moisture > 80: return "EXCITED"
    elif moisture > 50: return "HAPPY"
    elif moisture > 30: return "SLEEPY"
    else: return "SAD"