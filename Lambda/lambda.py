import json
import boto3

def lambda_handler(event, context):
    
  session = boto3.Session()
  honeycode_client = session.client('honeycode', region_name = 'us-west-2')   # connect to AWS Honeycode
  
  clientId = event['state']['reported']['clientidStatus'] # IoT client id from reported shadow document

  workbookId = 'ecf97bf3-57d1-48c2-a1e4-2886fe4df3ff' # NOTE: Your Workbook Id in Honeycode
  tableId = '654cf941-7739-40fc-a43e-c8609132c9c5'    # NOTE: Your WorkOrders Table Id within the Workbook
  columnId = 'b983ffd4-61a7-4f89-944b-13916cb13803'   # NOTE: The Status column within the WorkOrders table

  response = honeycode_client.query_table_rows(
    workbookId = workbookId,
    tableId = tableId,
    filterFormula = { 'formula': '=Filter(A_WorkOrders,"A_WorkOrders[IoT]=%","' + clientId + '")'} # gets the row where our IoT id is entered into the IoT column
  )

  rowId = response['rows'][0]['rowId']  # gets the row

  response = honeycode_client.batch_update_table_rows(  # updates 
    workbookId = workbookId,
    tableId = tableId,
    rowsToUpdate = [
      {
        'rowId': rowId,
        'cellsToUpdate': {
          columnId : { 'fact': 'Cleaned' } # set cell to: Cleaned
        }
      }
    ])
  
  return {
    'statusCode': 200,
    'body': json.dumps('SUCCESS')
  }
    
# testing
# testevent = {'state': {'reported': {'timestampStatus': '2021-08-15 09:38:42', 'clientidStatus': '0123456789abcdef', 'cleaningStatus': 'CLEANED'}}, 'metadata': {'reported': {'timestampStatus': {'timestamp': 1629020386}, 'clientidStatus': {'timestamp': 1629020386}, 'cleaningStatus': {'timestamp': 1629020386}}}, 'version': 7348, 'timestamp': 1629020386, 'clientToken': '0123456789abcdef-4'}
# print(lambda_handler(testevent, ''))
