import time
from datetime import datetime , timedelta
import pymongo
from pymongo import MongoClient


def StrToAscii(n):
    assert(type(n) == str)
    return [ord(c) for c in n]
def TimeStamp():
    return f"T:{datetime.now()}"
def Log(text,logfile):
    #print(f"T:{datetime.now()} {text}")
    if logfile:
        logfile.write(f"{text}\n")
        logfile.flush()
def writeCSV(data,logfile):
    # if logfile is not None:
    #     for i in txt_list:
    #         logfile.write(str(i))
    #         logfile.write(',')
    #     logfile.write('\n')
    #     logfile.flush()

    not_write_to_file = ["device"]

    if logfile is not None:
        for key,item in data.items():
            if key not in not_write_to_file:
                logfile.write(str(item))
                logfile.write(',')
        logfile.write('\n')
        logfile.flush()

def printTime(text):
        print(f"{TimeStamp()} {text}")

def upload_to_mongo(post):
    
    try:
        cluster = MongoClient("mongodb+srv://Sixsamurai:0620456803@cluster0.atzue.mongodb.net/myFirstDatabase?retryWrites=true&w=majority")
        db = cluster["Project"]
        collection = db["test1"]

        result = collection.insert_one(post)
        if result.acknowledged:
            #printT("Data upload to server successful")
            return 0
        else:
            #printT("fail to upload data")
            return 1
    except:
        #printT("error to connect to server")
        return 2

