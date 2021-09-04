import json
import boto3

def lambda_handler(event, context):
    
  session = boto3.Session()
  honeycode_client = session.client('honeycode', region_name = 'us-west-2')   # connect to AWS Honeycode
  
  clientid = event['state']['reported']['clientidStatus'] # IoT client id from reported shadow document

  response = honeycode_client.query_table_rows(
    workbookId = 'ecf97bf3-57d1-48c2-a1e4-2886fe4df3ff',  # NOTE: Your Workbook Id in Honeycode
    tableId = '654cf941-7739-40fc-a43e-c8609132c9c5',     # NOTE: Your WorkOrders Table Id within the Workbook
    filterFormula = { 'formula': '=Filter(A_WorkOrders,"A_WorkOrders[IoT]=%","' + clientid + '")'} # gets the row where our IoT id is entered into the IoT column
  )

  rowId = response['rows'][0]['rowId']  # gets the row

  response = honeycode_client.batch_update_table_rows(  # updates 
    workbookId = 'ecf97bf3-57d1-48c2-a1e4-2886fe4df3ff',  # NOTE: Your Workbook Id in Honeycode
    tableId = '654cf941-7739-40fc-a43e-c8609132c9c5',     # NOTE: Your Table Id within the Workbook
    rowsToUpdate = [
      {
        'rowId': rowId,
        'cellsToUpdate': {
          'b983ffd4-61a7-4f89-944b-13916cb13803': { 'fact': 'Cleaned' } # NOTE: the Status column in the row within the WorkOrders table to: Cleaned
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
