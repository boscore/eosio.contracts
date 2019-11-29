# coding:utf-8
#!/usr/bin/env python3
import json
import os
import requests
import sys
import time
import datetime
import multiprocessing



csvFile="./unactive_airdrop_accounts.csv"
nodes_endpoint = "http://127.0.0.1:380"


G_SUCCESS = 0
G_TIMEOUT = 1
G_ERROR = 2
G_NONEXIST = 3
G_NOEQUAL = 4

def get_burnbos_info(actinfo):
    if not actinfo:
        return [G_SUCCESS, (None, None)]
    account, quantity = actinfo
    url = nodes_endpoint + "/v1/chain/get_table_rows"
    params = {"json":True,"code":"burn.bos","scope":account,"table":"accounts","table_key":"","lower_bound":"",
            "upper_bound":"","limit":10,"key_type":"","index_position":"","encode_type":"dec","reverse":False,"show_payer":False}
    result, res_json = [G_SUCCESS, (account, quantity)], []
    try:
        res = requests.post(url, data=json.dumps(params), timeout=6)
        res_json = json.loads(res.text)
    except requests.exceptions.Timeout:
        result[0] = G_TIMEOUT
        return result
    except Exception as e:
        print('Exception: failed to get burn balance for:', account, e)
        result[0] = G_ERROR
        return result
    # res_json: 
    #   [0, {u'rows': [{u'account': u'grikovbtvfea', u'is_burned': 0, u'quantity': u'0.5000 BOS'}], u'more': False}]
    #   [0, {u'rows': [], u'more': False}]
    if not res_json or 'rows' not in res_json or len(res_json['rows']) == 0:
        result[0] = G_NONEXIST
        return result
    row = res_json['rows'][0]
    if not row['account'] or row['quantity'] != quantity:
        result[0] = G_NOEQUAL
    return result

def check_result(result, error_list, timeout_list, nonexist_list, noequal_list):
    if result[0] == G_ERROR:
        error_list.add(result[1])
    elif result[0] == G_TIMEOUT:
        timeout_list.add(result[1])
    elif result[0] == G_NONEXIST:
        nonexist_list.add(result[1])
    elif result[0] == G_NOEQUAL:
        noequal_list.add(result[1])


if __name__ == '__main__':
    print("Starting .... ",  datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f') ) 

    # Start the validation
    cpu_count = multiprocessing.cpu_count()
    process_pool = multiprocessing.Pool(processes=cpu_count)

    batch_size, accounts, cur_line = 1000, [], 0
    error_list, timeout_list, nonexist_list, noequal_list = [], [], [], []
    with open(csvFile, 'r') as f:
        batch_lines, line_count = [None] * batch_size, 0
        for line in f.readlines():
            batch_lines[line_count] = line.strip('\n').split(",")
            line_count += 1
            cur_line += 1
            if line_count<batch_size:
                continue
            results = process_pool.map(get_burnbos_info, batch_lines, cpu_count)
            for item in results:
                check_result(item, error_list, timeout_list, nonexist_list, noequal_list)
            print('handled accounts:', cur_line)
            batch_lines, line_count = [None] * batch_size, 0

        if line_count > 0:
            results = process_pool.map(get_burnbos_info, batch_lines, cpu_count)
            for item in results:
                check_result(item, error_list, timeout_list, nonexist_list, noequal_list)
        
        # try the TIMEOUT accounts
        try_timeout_list = []
        if timeout_list:
            print('Try for timeout accounts ... ', len(timeout_list))
            results = process_pool.map(get_burnbos_info, timeout_list, cpu_count)
            for item in results:
                check_result(item, error_list, try_timeout_list, nonexist_list, noequal_list)
        
        # Final result
        if error_list:
            print('\nERROR ACCOUNTS:\n', error_list)
        if try_timeout_list:
            print('\nTIMEOUT ACCOUNTS:\n', try_timeout_list)
        if nonexist_list:
            print('\nNONEXIST ACCOUNTS:\n', nonexist_list)
        if noequal_list:
            print('\nNOEQUAL ACCOUNTS:\n', noequal_list)

    print("Ending .... ",  datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f') ) 
    process_pool.close()
    process_pool.join()