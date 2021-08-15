import json
import boto3

def lambda_handler(event, context):
    
  session = boto3.Session()
  honeycode_client = session.client('honeycode', region_name = 'us-west-2')
  
  clientid = event['state']['reported']['clientidStatus']

  response = honeycode_client.query_table_rows(
    workbookId = 'ecf97bf3-57d1-48c2-a1e4-2886fe4df3ff',
    tableId = '654cf941-7739-40fc-a43e-c8609132c9c5',
    filterFormula = { 'formula': '=Filter(A_WorkOrders,"A_WorkOrders[IoT]=%","' + clientid + '")'} 
  )

  rowId = response['rows'][0]['rowId']

  response = honeycode_client.batch_update_table_rows(
    workbookId = 'ecf97bf3-57d1-48c2-a1e4-2886fe4df3ff',
    tableId = '654cf941-7739-40fc-a43e-c8609132c9c5',
    rowsToUpdate = [
      {
        'rowId': rowId,
        'cellsToUpdate': {
          'b983ffd4-61a7-4f89-944b-13916cb13803': { 'fact': 'Cleaned' }
        }
      }
    ])
  
  return {
    'statusCode': 200,
    'body': json.dumps('SUCCESS')
  }
    
# testing
# testevent = {'state': {'reported': {'timestampStatus': '2021-08-15 09:38:42', 'clientidStatus': '012376a6f0532c5e', 'cleaningStatus': 'CLEANED'}}, 'metadata': {'reported': {'timestampStatus': {'timestamp': 1629020386}, 'clientidStatus': {'timestamp': 1629020386}, 'cleaningStatus': {'timestamp': 1629020386}}}, 'version': 7348, 'timestamp': 1629020386, 'clientToken': '012376a6f0532c5e01-4'}
# print(lambda_handler(testevent, ''))
